// pipeline-graphic.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "gpu.h"

// =====================================================================
// BUFFER TRIANGLES  (prod/cons classique)
// =====================================================================
#define NB_TRIANGLES 1000
struct Triangle triangles[NB_TRIANGLES];
int triangle_read_index = 0;
int triangle_write_index = 0;
pthread_mutex_t triangle_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t triangle_sem_prod;   // (EMPTY) nb places libres
sem_t triangle_sem_conso;  // (FULL)  nb triangles dispo

// =====================================================================
// BUFFER FRAGMENTS  (prod/cons classique)
// =====================================================================
#define NB_FRAGMENTS 10000
struct Fragment fragments[NB_FRAGMENTS];
int fragment_read_index = 0;
int fragment_write_index = 0;
pthread_mutex_t fragment_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t fragment_sem_prod;   // (EMPTY) nb places libres
sem_t fragment_sem_conso;  // (FULL)  nb fragments dispo

// Nombres de threads par défaut
int nb_rasters = 4;
int nb_shaders = 4;

// =====================================================================
// ✅ TP8 - AJOUT/MODIF : BARRIÈRES pour synchroniser les étages
// =====================================================================
// barrier_rasters : rendez-vous entre triangle_stage + tous les rasters
// barrier_shaders : rendez-vous entre triangle_stage + tous les shaders
// barrier_all     : rendez-vous global (triangle + rasters + shaders) pour repartir ensemble
pthread_barrier_t barrier_rasters;
pthread_barrier_t barrier_shaders;
pthread_barrier_t barrier_all;

// =====================================================================
// STAGE 1 : triangle_stage (1 thread producteur de triangles)
// =====================================================================
static void *triangle_stage(void *a)
{
  struct Triangle triangle;

  // crée le générateur (fourni par gpu.c)
  void *triangle_creator = triangle_creator_init();

  while (triangle_creator_get_next_triangle(triangle_creator, &triangle))
  {
    // ------------------ PROD/CONS classique : push triangle ------------------
    sem_wait(&triangle_sem_prod);                   // attendre une place libre
      pthread_mutex_lock(&triangle_mutex);          // section critique index+table
        triangles[triangle_write_index] = triangle; // écrire
        triangle_write_index = (triangle_write_index + 1) % NB_TRIANGLES; // avancer FIFO
      pthread_mutex_unlock(&triangle_mutex);
    sem_post(&triangle_sem_conso);                  // signal : 1 triangle dispo

#if 1
    // =================================================================
    // ✅ TP8 - MODIF : Gestion d'un "triangle de synchronisation"
    // =================================================================
    // Un triangle "sync" n'est pas un vrai triangle : il sert à forcer un flush
    // du pipeline (ex: changement de couleur). On doit s'assurer que tous les
    // rasters + shaders passent un point de rendez-vous avant de repartir.
    if (triangle_is_sync(&triangle))
    {
      int i;

      // 1) S'assurer que CHAQUE raster thread reçoit un triangle sync.
      //    On vient d'en produire 1 juste avant le if, donc il en manque nb_rasters-1.
      for (i = 0; i < nb_rasters - 1; i++)
      {
        sem_wait(&triangle_sem_prod);
          pthread_mutex_lock(&triangle_mutex);
            triangles[triangle_write_index] = triangle;
            triangle_write_index = (triangle_write_index + 1) % NB_TRIANGLES;
          pthread_mutex_unlock(&triangle_mutex);
        sem_post(&triangle_sem_conso);
      }

      // 2) Attendre que tous les rasters aient consommé leur triangle sync et soient "arrivés"
      //    au point de synchro (raster_stage fait aussi barrier_rasters quand il voit le sync).
      pthread_barrier_wait(&barrier_rasters);

      // 3) Envoyer nb_shaders fragments sync pour réveiller tous les shader threads.
      //    (chaque shader doit en consommer un pour venir à la barrière)
      for (i = 0; i < nb_shaders; i++)
      {
        sem_wait(&fragment_sem_prod);
          pthread_mutex_lock(&fragment_mutex);
            fragments[fragment_write_index] = fragment_sync(); // fragment spécial sync
            fragment_write_index = (fragment_write_index + 1) % NB_FRAGMENTS;
          pthread_mutex_unlock(&fragment_mutex);
        sem_post(&fragment_sem_conso);
      }

      // 4) Attendre que tous les shaders aient consommé leur fragment sync et soient "arrivés"
      pthread_barrier_wait(&barrier_shaders);

      // 5) Pipeline "vidé" : reset des indices (on repart d’un état propre)
      //    (dans l’idée, après les barrières, il ne reste plus de vieux triangles/fragments à traiter)
      triangle_read_index = triangle_write_index = 0;
      fragment_read_index = fragment_write_index = 0;

      // 6) Relancer tout le monde en même temps (rasters + shaders + triangle_stage)
      //    raster_stage et shader_stage attendent aussi barrier_all après leur sync.
      pthread_barrier_wait(&barrier_all);
    }
#endif
  }

  // libération + génération de l’image finale (fait dans gpu.c)
  triangle_creator_destroy(triangle_creator);
  return a;
}

// =====================================================================
// STAGE 2 : raster_stage (nb_rasters threads)
// Consomme triangles -> produit fragments
// =====================================================================
void *raster_stage(void *a)
{
  while (1)
  {
    void *raster;
    struct Triangle triangle;
    struct Fragment fragment;

    // ------------------ PROD/CONS classique : pop triangle ------------------
    sem_wait(&triangle_sem_conso);                  // attendre triangle dispo
      pthread_mutex_lock(&triangle_mutex);
        triangle = triangles[triangle_read_index];  // lire
        triangle_read_index = (triangle_read_index + 1) % NB_TRIANGLES;
      pthread_mutex_unlock(&triangle_mutex);
    sem_post(&triangle_sem_prod);                   // rendre une place

#if 1
    // =================================================================
    // ✅ TP8 - MODIF : si triangle sync -> pas de rasterisation
    // =================================================================
    if (triangle_is_sync(&triangle))
    {
      // 1) Signaler au triangle_stage : "je suis arrivé au point de synchro"
      pthread_barrier_wait(&barrier_rasters);

      // 2) Attendre que triangle_stage finisse son flush/reset puis relance
      pthread_barrier_wait(&barrier_all);
    }
    else
#endif
    {
      // Rasterisation normale : triangle -> fragments
      raster = raster_init(&triangle);

      while (raster_get_next_fragment(raster, &fragment))
      {
        // ------------------ PROD/CONS classique : push fragment ------------------
        sem_wait(&fragment_sem_prod);
          pthread_mutex_lock(&fragment_mutex);
            fragments[fragment_write_index] = fragment;
            fragment_write_index = (fragment_write_index + 1) % NB_FRAGMENTS;
          pthread_mutex_unlock(&fragment_mutex);
        sem_post(&fragment_sem_conso);
      }

      // ⚠️ Petite amélioration : si l’API existe, il faut libérer raster
      // raster_destroy(raster);
    }
  }
  return a;
}

// =====================================================================
// STAGE 3 : shader_stage (nb_shaders threads)
// Consomme fragments -> gpu_shader(fragment)
// =====================================================================
void *shader_stage(void *a)
{
  while (1)
  {
    struct Fragment fragment;

    // ------------------ PROD/CONS classique : pop fragment ------------------
    sem_wait(&fragment_sem_conso);                  // attendre fragment dispo
      pthread_mutex_lock(&fragment_mutex);
        fragment = fragments[fragment_read_index];  // lire
        fragment_read_index = (fragment_read_index + 1) % NB_FRAGMENTS;
      pthread_mutex_unlock(&fragment_mutex);
    sem_post(&fragment_sem_prod);                   // rendre une place

#if 1
    // =================================================================
    // ✅ TP8 - MODIF : si fragment sync -> pas de shader
    // =================================================================
    if (fragment_is_sync(&fragment))
    {
      // 1) Signaler au triangle_stage : "je suis arrivé au point de synchro"
      pthread_barrier_wait(&barrier_shaders);

      // 2) Attendre la relance globale
      pthread_barrier_wait(&barrier_all);
    }
    else
#endif
    {
      // Shader normal
      gpu_shader(fragment);
    }
  }

  return a;
}

// =====================================================================
// main : init + création threads
// =====================================================================
int main(int argc, char *argv[])
{
  int i;

  // Lecture args : nb_rasters nb_shaders
  if (argc > 1)
  {
    nb_rasters = atoi(argv[1]);
    if (argc > 2)
      nb_shaders = atoi(argv[2]);
  }

  // Bornes de sécurité
  if (nb_rasters < 1 ) nb_rasters = 1;
  if (nb_rasters > 50) nb_rasters = 50;
  if (nb_shaders < 1 ) nb_shaders = 1;
  if (nb_shaders > 100)nb_shaders = 100;

  pthread_t t_tri;
  pthread_t t_rasters[nb_rasters];
  pthread_t t_shaders[nb_shaders];

  // =====================================================================
  // ✅ TP8 - AJOUT : initialisation des barrières
  // =====================================================================
  // rasters + triangle_stage => nb_rasters + 1
  pthread_barrier_init(&barrier_rasters, NULL, nb_rasters + 1);
  // shaders + triangle_stage => nb_shaders + 1
  pthread_barrier_init(&barrier_shaders, NULL, nb_shaders + 1);
  // tout le monde => nb_rasters + nb_shaders + 1
  pthread_barrier_init(&barrier_all, NULL, nb_shaders + nb_rasters + 1);

  // =====================================================================
  // ✅ TP8 - AJOUT : init sémaphores prod/cons triangles/fragments
  // =====================================================================
  sem_init(&triangle_sem_conso, 0, 0);
  sem_init(&triangle_sem_prod, 0, NB_TRIANGLES);

  sem_init(&fragment_sem_conso, 0, 0);
  sem_init(&fragment_sem_prod, 0, NB_FRAGMENTS);

  // Création des threads
  pthread_create(&t_tri, NULL, triangle_stage, NULL);

  for (i = 0; i < nb_rasters; i++)
    pthread_create(&t_rasters[i], NULL, raster_stage, NULL);

  for (i = 0; i < nb_shaders; i++)
    pthread_create(&t_shaders[i], NULL, shader_stage, NULL);

  // On attend seulement triangle_stage :
  // quand il finit, l’image est écrite (les autres threads ne sont plus utiles).
  pthread_join(t_tri, NULL);

  // Destruction des barrières (propre)
  pthread_barrier_destroy(&barrier_rasters);
  pthread_barrier_destroy(&barrier_shaders);
  pthread_barrier_destroy(&barrier_all);

  return 0;
}