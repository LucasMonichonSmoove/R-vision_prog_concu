#include "../dijkstra.h"
#include "data.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    // mutextable : protection SHM
    int mutextable = sem_get(1);

    // mutexwrite : je signale au comparateur que j'ai écrit table[1]
    int mutexwrite = sem_get(3);

    // mutexread : j'attends que le comparateur ait fini
    int mutexread = sem_get(5);

    // SHM
    int shmidtable = shmget(TABLEKEY, TABLESIZE*sizeof(int), 0);
    int* table = shmat(shmidtable, NULL, 0);

    // Tableau local fixe
    int tableLocale[3] = {4,69,32};

    int min = 100;      // valeur min trouvée
    int indexMin;       // index de min

    printf("base : %d,%d,%d\n",tableLocale[0],tableLocale[1],tableLocale[2]);

    while(1){
        // Recherche du min
        // (⚠️ pareil : min pas réinitialisé à chaque tour, mais tu le remets après swap)
        for(int i = 0; i<3; i++){
            if(tableLocale[i]<min){
                min = tableLocale[i];
                indexMin = i;
            }
        }

        // Écrit le min dans table[1]
        P(mutextable);
        table[1] = min;
        printf("dépot de %d dans table[1]\n",min);
        V(mutextable);

        // Signal au comparateur : "j'ai écrit"
        V(mutexwrite);

        // Attend la décision du comparateur
        P(mutexread);

        // Lit le flag et la nouvelle valeur
        P(mutextable);

        if(table[2] == -1){
            V(mutextable);
            break;
        }

        // table[1] contient la nouvelle valeur que MIN doit récupérer après swap
        tableLocale[indexMin] = table[1];

        // Mise à jour de min pour le tour suivant
        min = tableLocale[indexMin];

        V(mutextable);
    }

    // Détache + affiche
    shmdt(table);

    // ⚠️ Ici tu affiches "max :" mais c’est le résultat de MIN (P2).
    printf("max : %d,%d,%d\n",tableLocale[0],tableLocale[1],tableLocale[2]);
}