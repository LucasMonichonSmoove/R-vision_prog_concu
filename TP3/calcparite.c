#include <stdio.h>      // (pas utilisé ici, mais utile pour debug/perror)
#include <unistd.h>     // fork, read, write, close, unlink
#include <stdlib.h>     // exit
#include <fcntl.h>      // open, O_RDONLY, O_WRONLY
#include <sys/types.h>  // types système
#include <sys/stat.h>   // mkfifo

// FIFOs d'entrée (depuis le filtre)
#define PAIR "/tmp/pair"
#define IMPAIR "/tmp/impair"

// FIFOs de retour des sommes (vers le générateur)
#define RETPAIR "/tmp/retpair"
#define RETIMPAIR "/tmp/retimpair"

int main()
{
    int dfr, df;     // df = fifo d'entrée, dfr = fifo de retour
    int number;      // valeur lue
    int somme = 0;   // accumulateur

    // On fork pour créer 2 processus :
    // - fils : gère les pairs (PAIR -> RETPAIR)
    // - père : gère les impairs (IMPAIR -> RETIMPAIR)
    if(fork() == 0){

        // ----- BRANCHE PAIR (fils) -----

        // Nettoie les anciennes FIFOs si présentes
        unlink(PAIR);
        unlink(RETPAIR);

        // Crée les FIFOs pair et retour pair
        if(mkfifo(PAIR,0644) == -1){ exit(-1); }
        if(mkfifo(RETPAIR,0644) == -1){ exit(-1); }

        // Ouvre PAIR en lecture (bloque tant que le filtre n'a pas ouvert en écriture)
        df = open(PAIR, O_RDONLY);

        // Ouvre RETPAIR en écriture (bloque tant que le générateur n'a pas ouvert en lecture)
        dfr = open(RETPAIR, O_WRONLY);

    } else {

        // ----- BRANCHE IMPAIR (père) -----

        // Nettoie les anciennes FIFOs si présentes
        unlink(IMPAIR);
        unlink(RETIMPAIR);

        // Crée les FIFOs impair et retour impair
        if(mkfifo(IMPAIR,0644) == -1){ exit(-1); }
        if(mkfifo(RETIMPAIR,0644) == -1){ exit(-1); }

        // Ouvre IMPAIR en lecture
        df = open(IMPAIR, O_RDONLY);

        // Ouvre RETIMPAIR en écriture
        dfr = open(RETIMPAIR, O_WRONLY);
    }

    // Boucle de somme : lit des ints jusqu'à la sentinelle -1
    while(1){

        // Lit un int depuis la FIFO df (pair ou impair)
        // ⚠ pas de test du retour read()
        read(df, &number, sizeof(int));

        // Si -1 : fin du flux pour cette branche
        if(number == -1){

            // Envoie la somme au générateur via la FIFO de retour
            write(dfr, &somme, sizeof(int));

            // Sortie de boucle -> fin du process
            break;
        }

        // Accumule
        somme += number;
    }

    // ⚠ Ici il manque des close(df); close(dfr); (propreté)
    return 0;
}