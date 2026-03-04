#include "../dijkstra.h"
#include "data.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

int main()
{
    // ⚠️ PROBLÈME : shmctl() attend un SHMID, pas une KEY.
    // Ici TABLEKEY est une clé (key_t), pas l'identifiant shmid.
    // Il faut faire shmget(...) pour récupérer le shmid puis shmctl(shmid, IPC_RMID,...)
    shmctl(TABLEKEY, IPC_RMID, NULL);  // ❌ pas correct en général

    // Suppression des sémaphores
    sem_delete(sem_get(1));
    sem_delete(sem_get(2));
    sem_delete(sem_get(3));
    sem_delete(sem_get(4));
    sem_delete(sem_get(5));
}