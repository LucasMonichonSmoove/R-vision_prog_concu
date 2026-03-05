#include <stdlib.h>     // atoi(), exit(), rand()
#include <stdio.h>      // printf()
#include <unistd.h>     // sleep()
#include <pthread.h>    // threads + mutex
#include <semaphore.h>  // sem_t, sem_init, sem_wait, sem_post

// Arguments passés à chaque thread
struct args
{
    int n;                  // numéro du thread
    int *waiters;           // pointeur vers le compteur "arrivés"
    int total;              // N total
    pthread_mutex_t mutex;  // ⚠️ ici tu COPIES le mutex (pas idéal)
    sem_t *sem;             // sémaphore partagé pour bloquer/réveiller
};

void *rdv(void* arg){
    struct args *argument = arg;

    // Travail simulé
    int time2wait = rand()%10;
    printf("%d dors %ds\n", argument->n, time2wait);
    sleep(time2wait);

    // Arrivée au rendez-vous
    printf("%d attend\n", argument->n);

    // Section critique : protège *waiters
    pthread_mutex_lock(&argument->mutex);

    // Un thread de plus est arrivé
    *argument->waiters = *argument->waiters + 1;

    // Si je suis le dernier à arriver :
    if(*argument->waiters == argument->total){

        // Je libère les autres threads (N-1 threads bloqués sur sem_wait)
        for(int i=0; i<argument->total-1; i++){
            sem_post(argument->sem);
        }

    }else{
        // Sinon je me bloque en attendant que le dernier me réveille
        sem_wait(argument->sem);
    }

    // Fin section critique
    pthread_mutex_unlock(&argument->mutex);

    // Après rendez-vous
    printf("%d wakeup\n", argument->n);
    return NULL;
}

int main(int argc, char **argv){
    if(argc < 2){
        exit(-1);
    }

    int N = atoi(argv[1]);     // nombre de threads

    int attente = 0;           // compteur "arrivés"

    pthread_t thread[N];       // threads

    pthread_mutex_t mutex;     // mutex global
    pthread_mutex_init(&mutex, NULL);

    sem_t semaphore;           // sémaphore global
    sem_init(&semaphore, 0, 0);// init à 0 => ceux qui wait bloquent

    struct args arguments[N];

    // Crée les threads
    for(int i = 0; i<N; i++){
        arguments[i].n = i;
        arguments[i].waiters = &attente;
        arguments[i].total = N;

        // ⚠️ ICI tu copies le mutex dans chaque args
        // => mieux : stocker un pointeur vers mutex
        arguments[i].mutex = mutex;

        // pointeur vers le sémaphore partagé
        arguments[i].sem = &semaphore;

        pthread_create(&thread[i], NULL, rdv, &arguments[i]);
    }

    // Attend fin de tous les threads
    for(int i = 0; i<N; i++){
        pthread_join(thread[i], NULL);
    }

    // Détruit les ressources
    pthread_mutex_destroy(&mutex);
    sem_destroy(&semaphore);
    return 0;
}