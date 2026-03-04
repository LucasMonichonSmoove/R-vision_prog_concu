#include "../dijkstra.h"   // sem_get(), P(), V()
#include "data.h"          // TABLEKEY, TABLESIZE

#include <stdlib.h>        // (pas indispensable ici mais OK)
#include <unistd.h>        // (pas indispensable ici mais OK)
#include <stdio.h>         // printf()
#include <sys/ipc.h>       // IPC
#include <sys/shm.h>       // shmget(), shmat(), shmdt()

int main()
{
    // --- Sémaphores ---
    // mutextable : protège l'accès à la mémoire partagée "table" (table[0], table[1], table[2])
    int mutextable = sem_get(1);

    // mutexwriteMax : signal "MAX a écrit sa valeur dans table[0]"
    int mutexwriteMax = sem_get(2);

    // mutexwriteMin : signal "MIN a écrit sa valeur dans table[1]"
    int mutexwriteMin = sem_get(3);

    // mutexreadMax : signal "comparateur a fini, MAX peut lire et mettre à jour son tableau"
    int mutexreadMax = sem_get(4);

    // mutexreadMin : signal "comparateur a fini, MIN peut lire et mettre à jour son tableau"
    int mutexreadMin = sem_get(5);

    // --- Mémoire partagée ---
    // On récupère le segment de SHM "table" (3 entiers dans ton design)
    int shmidtable = shmget(TABLEKEY, TABLESIZE*sizeof(int), 0);

    // On attache le segment en mémoire pour obtenir un pointeur int*
    int* table = shmat(shmidtable, NULL, 0);

    // table[2] sert de "flag d'arrêt" :
    // 0 => on continue
    // -1 => stop (plus d'échange)
    // Tu forces au début une valeur != -1 pour éviter un stop immédiat.
    P(mutextable);
    table[2] = 0;
    V(mutextable);

    while(1){
        // Attendre que MAX ET MIN aient écrit leurs valeurs :
        // - MAX fait V(mutexwriteMax) après avoir écrit table[0]
        // - MIN fait V(mutexwriteMin) après avoir écrit table[1]
        P(mutexwriteMax);
        P(mutexwriteMin);

        // Section critique : on manipule la table partagée
        P(mutextable);

        // Condition d'arrêt :
        // Si max(P1) < min(P2), alors tout est "bien rangé" :
        // P1 a déjà des valeurs <= P2 => plus besoin d'échanges
        if(table[0] < table[1]){
            table[2] = -1;       // flag stop
            V(mutextable);       // on libère la table

            // On réveille les deux processus pour qu'ils voient le -1 et s'arrêtent
            V(mutexreadMax);
            V(mutexreadMin);

            break;               // sortie de boucle
        }

        // Sinon, on échange table[0] et table[1]
        // Ici tu fais un swap "sans variable temporaire" (addition/soustraction)
        // (Ça marche si pas d'overflow ; plus simple au partiel : variable tmp)
        table[0] += table[1];
        table[1] = table[0] - table[1];
        table[0] = table[0] - table[1];

        // Fin section critique
        V(mutextable);

        // Réveiller MIN et MAX pour qu'ils récupèrent leurs nouvelles valeurs
        V(mutexreadMin);
        V(mutexreadMax);
    }

    // Détache la mémoire
    shmdt(table);
}