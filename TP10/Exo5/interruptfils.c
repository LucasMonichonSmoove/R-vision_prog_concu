#include <stdio.h>     // puts()
#include <string.h>    // memset()
#include <unistd.h>    // fork(), sleep()
#include <signal.h>    // sigaction(), SIGINT
#include <stdlib.h>    // (pas utilisé ici -> peut être retiré)

void redirect(int signum) {
    if (signum == SIGINT) {             // si réception de Ctrl+C
        puts("interception SIGINT");    // affiche un message (TP OK)
        // pas de exit() => le fils continue, c'est ce que veut l'exercice
    }
}

int main(void)
{
    pid_t fils = fork();                 // crée le fils

    if (fils != 0) {                     // processus père (fork() retourne PID du fils)
        for (int i = 0; i < 5; i++) {    // le père affiche 5 fois puis termine
            puts("père");
            sleep(1);
        }
        // le père termine => si le fils tourne encore, il devient orphelin
    } else {                             // processus fils (fork() retourne 0)

        // Installation du handler SIGINT uniquement dans le fils
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = redirect;
        sigaction(SIGINT, &act, NULL);

        while (1) {                      // boucle infinie du fils
            puts("fils");
            sleep(1);
        }
    }
    return 0;
}