#include "../dijkstra.h"
#include "data.h"

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(void){
    // mutex table : protège la mémoire partagée table[]
    sem_create(1,1);

    // writeMax : signal "max a écrit"
    sem_create(2,0);

    // writeMin : signal "min a écrit"
    sem_create(3,0);

    // readMax : signal "comparateur a fini, max peut lire"
    sem_create(4,0);

    // readMin : signal "comparateur a fini, min peut lire"
    sem_create(5,0);

    // Création de la SHM (table de TABLESIZE int)
    // IPC_EXCL => nécessite clean avant init
    shmget(TABLEKEY,TABLESIZE*sizeof(int),IPC_CREAT|IPC_EXCL|0600);
}