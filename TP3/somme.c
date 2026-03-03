#include <stdio.h>      // printf
#include <unistd.h>     // write, read, close
#include <stdlib.h>     // rand
#include <fcntl.h>      // open, O_WRONLY, O_RDONLY
#include <sys/types.h>  // types système
#include <sys/stat.h>   // (pas indispensable ici)

// FIFO vers filtre (générateur -> filtre)
#define FILTRE "/tmp/filtre"

// Ici les macros sont mal nommées : ce sont les FIFOs de RETOUR
#define PAIR "/tmp/retpair"
#define IMPAIR "/tmp/retimpair"

int main()
{
    int dff, dfp, dfi; // dff = fifo filtre en écriture, dfp/dfi = retours en lecture

    // Ouvre le FIFO filtre en écriture.
    // open(..., O_WRONLY) bloque tant que le filtre n'a pas ouvert en lecture.
    dff = open(FILTRE, O_WRONLY);

    int number;

    // Génère 50 nombres + envoie -1 à la fin (51 itérations)
    for(int i = 0; i < 51; i++){

        // Dernière itération : sentinelle -1 (fin)
        if(i == 50){
            number = -1;
        } else {
            // Nombre pseudo-aléatoire 0..99
            // ⚠ pas de srand() => séquence souvent identique à chaque run
            number = rand()%100;
        }

        // Envoie le nombre au filtre
        if(write(dff, &number, sizeof(int)) != -1){
            printf("%d : Envoyé\n", number);
        } else {
            printf("%d : Erreur d'envoi\n", number);
        }
    }

    // Ouvre les FIFOs de retour (lecture) :
    // bloque tant que calcparite n'a pas ouvert en écriture
    dfp = open(PAIR, O_RDONLY);
    dfi = open(IMPAIR, O_RDONLY);

    int sommePair;
    int sommeImpair;

    // Lit la somme pair et la somme impair
    // ⚠ pas de test de read()
    read(dfp, &sommePair, sizeof(int));
    read(dfi, &sommeImpair, sizeof(int));

    // Affiche les résultats
    printf("Somme des nombres pairs : %d\nSomme des nombres impairs : %d\nTotal : %d\n",
           sommePair, sommeImpair, sommeImpair + sommePair);

    // Ferme les descripteurs
    close(dff);
    close(dfp);
    close(dfi);

    return 0;
}