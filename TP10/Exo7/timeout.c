#include <stdio.h>     // puts(), fgets(), sscanf()
#include <string.h>    // memset()
#include <unistd.h>    // alarm()
#include <signal.h>    // sigaction(), SIGALRM
#include <stdlib.h>    // exit()

// Fonction qui gère la "boucle" de saisie utilisateur
// (ici tu l'as faite en récursion : si l'entrée n'est pas un int, on rappelle boucle())
void boucle(){
    alarm(5);                      // arme un timeout de 5 secondes (envoie SIGALRM au bout de 5s)
    puts("Svp un entier : ");      // affiche la consigne (puts ajoute un '\n')

    char c[100];                   // buffer pour stocker la ligne saisie
    fgets(c, sizeof(c), stdin);    // lit une ligne au clavier (string), évite de "planter" sur une lettre

    int value;                     // variable qui recevra l'entier si conversion OK
    int reussi = sscanf(c, "%d", &value); // tente de convertir la chaîne en entier
                                         // renvoie 1 si un int a été lu, sinon 0

    if (reussi == 1) {             // si conversion réussie
        alarm(0);                  // désarme le timeout (sinon SIGALRM arriverait plus tard)
        puts("Ok merci !!");       // message de fin
    } else {
        boucle();                  // sinon, on recommence (⚠️ récursif : empile les appels)
    }
}

// Handler SIGALRM : appelé automatiquement quand le timer alarm() expire
void redirect(int signum) {
    if (signum == SIGALRM) {       // si on reçoit SIGALRM (timeout)
        puts("Trop tard !!");      // affiche le message demandé
        exit(0);                   // termine le programme immédiatement
    }
}

int main(void)
{
    // Installation du handler SIGALRM
    struct sigaction act;          // structure de configuration d’un signal
    memset(&act, 0, sizeof(act));  // initialise tous les champs à 0
    act.sa_handler = redirect;     // fonction à appeler quand SIGALRM arrive
    sigaction(SIGALRM, &act, NULL);// installe le handler pour SIGALRM

    puts("Entrez un entier en moins de 5 secondes"); // message initial

    boucle();                      // lance la logique de saisie (avec timeout)

    return 0;                      // fin normale (souvent jamais atteinte si exit() dans handler)
}