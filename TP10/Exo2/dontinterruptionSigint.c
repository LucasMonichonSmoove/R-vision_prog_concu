#include <stdio.h>    // puts()
#include <string.h>   // memset()
#include <unistd.h>   // (pas utilisé ici -> peut être retiré)
#include <signal.h>   // sigaction, SIGINT, SIG_IGN
#include <stdlib.h>   // (pas utilisé ici -> peut être retiré)

int main(void)
{
    struct sigaction act;               // structure qui décrit quoi faire quand un signal arrive
    memset(&act, 0, sizeof(act));       // initialise tous les champs à 0 (bonne pratique)

    act.sa_handler = SIG_IGN;           // SIG_IGN = "ignore ce signal" (aucun handler appelé)
    sigaction(SIGINT, &act, NULL);      // applique cette règle au signal SIGINT (Ctrl+C)

    while(1){                           // boucle infinie
        puts("affiche");                // affiche en continu
        // NB: ça spam très vite, normal
    }

    puts("end");                        // jamais exécuté (boucle infinie)
    return 0;
}