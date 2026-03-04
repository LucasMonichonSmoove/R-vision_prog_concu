#include "../dijkstra.h"
#include "data.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    // mutextable : protège les accès à la table partagée
    int mutextable = sem_get(1);

    // mutexwrite : je signale au comparateur que j'ai écrit table[0]
    int mutexwrite = sem_get(2);

    // mutexread : j'attends que le comparateur ait fini (swap/stop)
    int mutexread = sem_get(4);

    // SHM "table"
    int shmidtable = shmget(TABLEKEY, TABLESIZE*sizeof(int), 0);
    int* table = shmat(shmidtable, NULL, 0);

    // Ton tableau local (ici 3 valeurs fixes)
    int tableLocale[3] = {15,89,3};

    int max = 0;        // valeur maximale trouvée
    int indexMax;       // index où se trouve max dans tableLocale

    printf("base : %d,%d,%d\n",tableLocale[0],tableLocale[1],tableLocale[2]);

    while(1){
        // Recherche du max dans tableLocale
        // (⚠️ petit détail : max n'est pas réinitialisé à chaque tour, mais tu le remets après swap)
        for(int i = 0; i<3; i++){
            if(tableLocale[i]>max){
                max = tableLocale[i];
                indexMax = i;
            }
        }

        // Écriture de max dans la mémoire partagée à la case 0
        P(mutextable);
        table[0] = max;
        printf("dépot de %d dans table[0]\n",max);
        V(mutextable);

        // Signal au comparateur : "j'ai écrit"
        V(mutexwrite);

        // Attendre que le comparateur ait fini son tour
        P(mutexread);

        // Reprendre la table pour lire le résultat (table[0] éventuellement modifié)
        P(mutextable);

        // Si table[2] == -1 => stop : plus d'échanges
        if(table[2] == -1){
            V(mutextable);
            break;
        }

        // Sinon, le comparateur a fait un swap :
        // table[0] contient la "nouvelle valeur" que MAX doit récupérer
        tableLocale[indexMax] = table[0];

        // Mise à jour de max (pour le tour suivant)
        max = tableLocale[indexMax];

        V(mutextable);
    }

    // Détache SHM + affiche résultat final
    shmdt(table);

    // ⚠️ Ici tu affiches "min :" mais c’est le résultat de MAX (P1).
    printf("min : %d,%d,%d\n",tableLocale[0],tableLocale[1],tableLocale[2]);
}