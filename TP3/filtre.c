#include <stdio.h>      // fprintf, printf, perror
#include <unistd.h>     // read, write, close, unlink
#include <stdlib.h>     // exit
#include <fcntl.h>      // open, O_RDONLY, O_WRONLY
#include <sys/types.h>  // types système
#include <sys/stat.h>   // mkfifo, permissions

// FIFO (tubes nommés) utilisés pour la communication
#define FILTRE "/tmp/filtre"   // générateur -> filtre
#define PAIR "/tmp/pair"       // filtre -> sommeur pair
#define IMPAIR "/tmp/impair"   // filtre -> sommeur impair

// Fonction d'erreur : affiche un message et termine le programme
void error(char* file) {
    fprintf(stderr,"Can't create %s\n",file);
    exit(1);
}

int main()
{
    int dff, dfp, dfi; // descripteurs de fichiers (fd) ouverts sur les FIFOs

    // Supprime un éventuel ancien FIFO /tmp/filtre (sinon mkfifo échoue)
    unlink(FILTRE);

    // Crée le FIFO /tmp/filtre avec droits 0644
    // (prw-r--r--) typiquement
    if(mkfifo(FILTRE,0644) == -1){
        error(FILTRE);
    }

    // Message de debug : indique que le filtre attend la production
    printf("En attente de données\n");

    // Ouvre /tmp/filtre en lecture : va bloquer tant qu'aucun writer n'a ouvert l'autre côté
    dff = open(FILTRE, O_RDONLY);

    // Ouvre les FIFOs pair/impair en écriture :
    // open(..., O_WRONLY) va bloquer tant que le lecteur (calcparite) n'est pas prêt
    dfp = open(PAIR, O_WRONLY);
    dfi = open(IMPAIR, O_WRONLY);

    int number;

    // Boucle infinie : lit les nombres et les route
    while(1){

        // Lit un int depuis le FIFO FILTRE
        // ⚠ ici on ne vérifie pas la valeur de retour de read (0/ -1)
        read(dff, &number, sizeof(int));

        // Debug : affiche le nombre reçu
        printf("%d\n", number);

        // -1 = sentinelle de fin : on doit arrêter tout le pipeline
        if(number == -1){

            // On propage la sentinelle aux deux branches (pair et impair)
            write(dfp, &number, sizeof(int));
            write(dfi, &number, sizeof(int));

            // On sort de la boucle -> fin du filtre
            break;
        }

        // Route vers pair/impair
        if(number % 2 == 0){
            printf("pair\n");              // debug
            write(dfp, &number, sizeof(int)); // envoie au FIFO pair
        } else {
            printf("impair\n");            // debug
            write(dfi, &number, sizeof(int)); // envoie au FIFO impair
        }
    }

    // Ferme les descripteurs
    close(dff);
    close(dfp);
    close(dfi);

    // Fin programme
    return 0;
}