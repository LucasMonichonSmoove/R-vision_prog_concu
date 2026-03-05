#include <stdlib.h>     // atoi(), exit()
#include <stdio.h>      // printf()
#include <unistd.h>     // sleep()
#include <pthread.h>    // pthread_* + pthread_barrier_*

// Structure passée à chaque thread
struct args
{
    int n;                     // numéro du thread (0..N-1)
    pthread_barrier_t* barrier;// pointeur vers la barrière partagée
};

// Fonction exécutée par chaque thread
void *rdv(void* arg){
    struct args *argument = arg;           // récupère les arguments

    int time2wait = rand()%10;             // durée "travail" (0..9)
    printf("%d dors %ds\n", argument->n, time2wait);
    sleep(time2wait);                      // simulation de travail

    printf("%d Attend\n", argument->n);    // arrivée au rendez-vous

    // Barrière : bloque tant que N threads ne sont pas arrivés ici
    pthread_barrier_wait(argument->barrier);

    // Après la barrière : tous les threads continuent (ordre non déterministe)
    printf("%d wakeup\n", argument->n);
    return NULL;
}

int main(int argc, char **argv){
    // Vérifie argument N
    if(argc < 2){
        exit(-1);
    }

    int N = atoi(argv[1]);                 // nombre de threads

    pthread_t thread[N];                   // tableau des threads
    pthread_barrier_t barrier;             // barrière

    // Initialise la barrière pour N threads
    pthread_barrier_init(&barrier, NULL, N);

    struct args arguments[N];              // arguments pour chaque thread

    // Crée N threads
    for(int i = 0; i<N; i++){
        arguments[i].n = i;                // id thread
        arguments[i].barrier = &barrier;   // même barrière pour tous
        pthread_create(&thread[i], NULL, rdv, &arguments[i]);
    }

    // Attend la fin de tous les threads
    for(int i = 0; i<N; i++){
        pthread_join(thread[i], NULL);
    }

    // Détruit la barrière
    pthread_barrier_destroy(&barrier);

    return 0;
}