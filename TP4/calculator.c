#include <stdio.h>      // printf, fprintf
#include <unistd.h>     // pipe, fork, read, write, close, _exit, getpid
#include <stdlib.h>     // atoi, srand, rand, exit
#include <sys/wait.h>   // wait

// Requête envoyée d'un client vers le serveur (via le tube commun mainTube)
struct requete_client_serveur {
    int clientPid;      // PID du client (sert à retrouver à quel client répondre)
    int nombre1;        // premier opérande
    int nombre2;        // deuxième opérande
    char operator;      // opérateur '+' ou '-'
};

// Réponse envoyée du serveur vers le client (via le tube dédié tubes[i])
struct resultat_client_serveur {
    int nombre1;        // rappelle le premier opérande
    int nombre2;        // rappelle le deuxième opérande
    int resultat;       // résultat du calcul
    char operator;      // rappelle l’opérateur
};

int main(int argc, char **argv)
{
    // Vérifie qu'on a bien l'argument N (nombre de clients)
    if (argc < 2) {
        // Affiche l'utilisation correcte du programme sur stderr
        fprintf(stderr, "Usage: %s <nb_clients>\n", argv[0]);
        // Quitte le programme avec un code d'erreur standard
        exit(EXIT_FAILURE);
    }

    // Convertit l'argument texte en entier : N = nombre de clients
    int n = atoi(argv[1]);

    // Vérifie que N est valide (>0)
    if (n <= 0) {
        // Message d'erreur
        fprintf(stderr, "N doit être > 0\n");
        // Quitte en erreur
        exit(EXIT_FAILURE);
    }

    // mainTube : tube commun dans le sens clients -> serveur
    // mainTube[0] = lecture, mainTube[1] = écriture
    int mainTube[2];

    // tubes[i] : tube dédié dans le sens serveur -> client i
    // tubes[i][0] = lecture (côté client), tubes[i][1] = écriture (côté serveur)
    int tubes[n][2];

    // Tableau associant l'index i au PID du client créé (pour faire PID -> i)
    int clientPids[n];

    // Crée le tube commun mainTube
    if (pipe(mainTube) == -1) {      // pipe() remplit mainTube[0] et mainTube[1]
        perror("pipe");              // affiche l'erreur système
        exit(EXIT_FAILURE);          // quitte en erreur
    }

    // Boucle : crée N clients + les N tubes de réponse
    for (int i = 0; i < n; i++) {

        // Crée le tube de réponse du client i (serveur -> client i)
        if (pipe(tubes[i]) == -1) {
            perror("pipe");          // erreur de création de pipe
            exit(EXIT_FAILURE);
        }

        // fork() crée un processus enfant (client)
        pid_t pid = fork();

        // Si fork échoue, pid < 0
        if (pid < 0) {
            perror("fork");          // affiche l'erreur
            exit(EXIT_FAILURE);
        }

        // Si pid == 0, on est dans le processus enfant => CLIENT i
        if (pid == 0) {
            // Initialise la génération pseudo-aléatoire avec une seed différente par client
            srand(getpid());

            // Le client n'a pas besoin de lire dans le tube commun (clients->serveur),
            // donc il ferme l'extrémité lecture
            close(mainTube[0]);

            // Ferme les FDs inutiles pour éviter les blocages (deadlocks) :
            // - le client n'écrit jamais sur les tubes de réponse
            // - le client ne lit que son tube à lui (tubes[i][0])
            for (int j = 0; j < n; j++) {
                close(tubes[j][1]);                 // ferme l'écriture (serveur->client) côté client
                if (j != i) close(tubes[j][0]);     // ferme la lecture des autres clients
            }

            // Déclare une requête locale (évite de partager une variable globale avec le parent)
            struct requete_client_serveur req;

            // Remplit le PID client (PID du process courant)
            req.clientPid = (int)getpid();

            // Génère deux nombres (0..9)
            req.nombre1 = rand() % 10;
            req.nombre2 = rand() % 10;

            // Choisit aléatoirement '+' ou '-'
            req.operator = (rand() % 2 == 0) ? '+' : '-';

            // Envoie la requête au serveur via l'extrémité écriture du tube commun
            write(mainTube[1], &req, sizeof(req));

            // Ferme l'écriture du tube commun (important : évite que le serveur attende)
            close(mainTube[1]);

            // Déclare une structure réponse locale
            struct resultat_client_serveur res;

            // Attend la réponse du serveur sur le tube dédié du client i
            read(tubes[i][0], &res, sizeof(res));

            // Ferme la lecture du tube de réponse (plus besoin)
            close(tubes[i][0]);

            // Affiche le résultat reçu
            printf("Client pid=%d : %d %c %d = %d\n",
                   req.clientPid,                 // PID du client
                   res.nombre1,                   // opérande 1
                   res.operator,                  // opérateur
                   res.nombre2,                   // opérande 2
                   res.resultat);                 // résultat

            // Quitte le client sans repasser par les handlers stdio du parent
            _exit(EXIT_SUCCESS);
        }

        // Si on est ici, pid > 0 => on est dans le serveur (process parent)

        // Sauvegarde le PID du client i pour pouvoir retrouver son index plus tard
        clientPids[i] = (int)pid;
    }

    // ===== SERVEUR (process parent) =====

    // Le serveur n'écrit jamais de requêtes, il lit seulement : ferme mainTube[1]
    close(mainTube[1]);

    // Le serveur n'a pas besoin de lire les tubes de réponse, il écrit dedans :
    // on ferme donc toutes les extrémités lecture tubes[i][0] côté serveur
    for (int i = 0; i < n; i++) {
        close(tubes[i][0]);
    }

    // Traite exactement N requêtes (une par client dans ce TP)
    for (int k = 0; k < n; k++) {

        // Lit une requête en provenance d'un des clients
        struct requete_client_serveur req;
        read(mainTube[0], &req, sizeof(req));

        // Calcule le résultat selon l'opérateur
        int result = 0;
        if (req.operator == '+') result = req.nombre1 + req.nombre2;
        else if (req.operator == '-') result = req.nombre1 - req.nombre2;

        // Prépare la structure réponse
        struct resultat_client_serveur res;
        res.nombre1 = req.nombre1;       // recopie opérande 1
        res.nombre2 = req.nombre2;       // recopie opérande 2
        res.operator = req.operator;     // recopie opérateur
        res.resultat = result;           // stocke le résultat

        // Retrouve l'index du client à partir du PID contenu dans la requête
        int idx = -1;
        for (int i = 0; i < n; i++) {
            if (clientPids[i] == req.clientPid) { // si PID match
                idx = i;                          // on a trouvé le bon client
                break;                            // on peut arrêter la recherche
            }
        }

        // Si on a trouvé un client valide, on envoie la réponse sur son tube dédié
        if (idx != -1) {
            write(tubes[idx][1], &res, sizeof(res));
        }
    }

    // Ferme la lecture du tube commun (plus besoin)
    close(mainTube[0]);

    // Ferme toutes les extrémités écriture des tubes de réponse (serveur -> clients)
    for (int i = 0; i < n; i++) close(tubes[i][1]);

    // Attend la fin des N clients (évite les zombies)
    for (int i = 0; i < n; i++) wait(NULL);

    // Fin normale
    return 0;
}