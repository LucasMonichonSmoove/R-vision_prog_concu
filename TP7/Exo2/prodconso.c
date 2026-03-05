#include <stdlib.h>      // atoi(), exit(), rand()
#include <unistd.h>      // sleep()
#include <stdio.h>       // printf()
#include <pthread.h>     // pthread_create/join + mutex
#include <semaphore.h>   // sem_t + sem_init/wait/post

// Arguments partagés entre tous les threads
struct args{
    sem_t *canRead;          // sémaphore "FULL" : nb d'éléments disponibles à lire
    sem_t *canWrite;         // sémaphore "EMPTY": nb de places libres pour écrire
    int *buffer;             // buffer partagé : [0..N-1] données + [N]=head + [N+1]=count
    int *N;                  // pointeur vers la taille du buffer (N)
    pthread_mutex_t mutex;   // ⚠️ PIÈGE : ici tu COPIES le mutex (pas idéal en threads)
};

// -------------------------
// Fonction Consommateur
// -------------------------
void *consoFct(void *arguments){
    struct args *args = arguments;   // cast des arguments

    int *tab = args->buffer;         // pointeur vers le buffer partagé
    int *N = args->N;                // pointeur vers N (taille)

    // Attend qu'il y ait AU MOINS 1 élément à lire.
    // Si canRead == 0 => buffer vide => ce thread se bloque ici.
    sem_wait(args->canRead);

    // Entrée en section critique : protège tab[*N] (head), tab[*N+1] (count) et les accès buffer
    pthread_mutex_lock(&args->mutex);

    // tab[*N] = head (index de lecture)
    // On lit la donnée FIFO à l'index head
    int data = tab[ tab[*N] ];

    // On mémorise l'index lu (juste pour l'affichage)
    int index = tab[*N];

    // tab[*N+1] = count (nombre d'éléments présents)
    // Après consommation : count--
    tab[*N+1] -= 1;

    // Après consommation : head = (head+1) % N (buffer circulaire)
    tab[*N] = (tab[*N] + 1) % *N;

    // Sortie de section critique
    pthread_mutex_unlock(&args->mutex);

    // Affiche la valeur consommée et l'index d'où elle vient
    printf("donnée reçue : %d sur index %d\n", data, index);

    // On rend une place libre => un producteur peut écrire.
    // C'est l'inverse de sem_wait(canWrite) côté prod.
    sem_post(args->canWrite);

    return NULL; // fin du thread conso (⚠️ ici le conso ne consomme qu'UNE fois)
}

// -------------------------
// Fonction Producteur
// -------------------------
void *prodFct(void *arguments){
    struct args *args = arguments;

    int *tab = args->buffer;
    int *N = args->N;

    // Produit une donnée (0..9)
    int random = rand()%10;

    // Simule un travail (délai) avant de produire
    // ⚠️ rand() n'est pas thread-safe selon plateformes, et pas seed ici (srand)
    sleep(random);

    // Attend qu'il y ait AU MOINS 1 place libre pour écrire.
    // Si canWrite == 0 => buffer plein => ce thread se bloque ici.
    sem_wait(args->canWrite);

    // Entrée en section critique
    pthread_mutex_lock(&args->mutex);

    // tab[*N]   = head
    // tab[*N+1] = count
    // L'index d'écriture FIFO (tail) = (head + count) % N
    // -> on écrit toujours à la "fin logique" de la file
    tab[(tab[*N] + tab[(*N)+1]) % *N] = random;

    // On mémorise l'index pour l'affichage (même calcul)
    int index = (tab[*N] + tab[(*N)+1]) % *N;

    // Après production : count++
    tab[(*N)+1] += 1;

    // Sortie section critique
    pthread_mutex_unlock(&args->mutex);

    // Signale qu'il y a maintenant 1 élément disponible à lire
    sem_post(args->canRead);

    // Affiche ce qui a été produit et à quel index
    printf("donnée écrite : %d sur index %d\n", random, index);

    return NULL; // fin du thread prod (⚠️ ici le prod ne produit qu'UNE fois)
}

// -------------------------
// main
// -------------------------
int main(int argc, char **argv)
{
    // Vérifie qu'on a l'argument N
    if(argc < 2){
        exit(-1);
    }

    // Taille du buffer
    int N = atoi(argv[1]);

    // --- Initialisation du mutex global ---
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    // --- Sémaphores ---
    // canWrite (EMPTY) : nb de places libres, init = N (buffer vide au début)
    sem_t canWrite;
    sem_init(&canWrite, 0, N);

    // canRead (FULL) : nb d'éléments dispo, init = 0
    sem_t canRead;
    sem_init(&canRead, 0, 0);

    // --- Buffer partagé entre threads ---
    // buffer[0..N-1] = données
    // buffer[N]      = head (index de lecture)
    // buffer[N+1]    = count (nb éléments)
    int buffer[N+2];

    // Initialisation head et count
    buffer[N] = 0;      // head = 0
    buffer[N+1] = 0;    // count = 0 (buffer vide)

    // Nombre de producteurs / consommateurs
    int nProd = 12;
    int nConso = 14;    // ⚠️ PIÈGE : 14 conso mais seulement 12 prod => 2 conso peuvent rester bloqués

    // Tableaux de threads
    pthread_t prod[nProd];
    pthread_t conso[nConso];

    // Structure args partagée (identique pour tous les threads)
    struct args args;
    args.canRead = &canRead;
    args.canWrite = &canWrite;
    args.buffer = buffer;
    args.N = &N;

    // ⚠️ PIÈGE : tu copies le mutex dans args
    // => la manière propre : mettre pthread_mutex_t* mutex; et args.mutex = &mutex;
    args.mutex = mutex;

    // Création des threads producteurs
    for(int i = 0; i<nProd; i++){
        pthread_create(&prod[i], NULL, prodFct, &args);
    }

    // Création des threads consommateurs
    for(int i = 0; i<nConso; i++){
        pthread_create(&conso[i], NULL, consoFct, &args);
    }

    // Attend la fin de tous les producteurs
    for(int i = 0; i<nProd; i++){
        pthread_join(prod[i], NULL);
    }

    // Attend la fin de tous les consommateurs
    // ⚠️ Si certains conso sont bloqués (pas assez de prod), join ne finira jamais.
    for(int i = 0; i<nConso; i++){
        pthread_join(conso[i], NULL);
    }

    // (Il manque le destroy : pthread_mutex_destroy + sem_destroy, mais pas indispensable pour l'exo)
}