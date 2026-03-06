
# 🧠 Fiche Révision – Programmation Concurrente / Systèmes (C)

---

# 1️⃣ Récupérer un argument N (nombre de threads)

Permet de récupérer un argument passé en ligne de commande.

Exemple :

    ./programme 5

Code :

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int main(int argc, char *argv[]) {

    // Vérifie que l'utilisateur a bien donné un argument
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <nombre_de_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Conversion de l'argument en entier
    int N = atoi(argv[1]);

    printf("Nombre de threads à créer : %d\n", N);
}
```

### ⚠️ À retenir pour le partiel

| Élément | Rôle |
|------|------|
| argc | nombre d'arguments |
| argv | tableau des arguments |
| argv[0] | nom du programme |
| argv[1] | premier argument utilisateur |
| atoi() | conversion string → int |

---

# 2️⃣ Conversion sécurisée avec sscanf

`atoi()` ne détecte pas les erreurs.  
On utilise donc souvent `sscanf`.

```c
int NUM_THREADS;

int nb_success = sscanf(argv[1], "%d", &NUM_THREADS);

if (nb_success == 1 && NUM_THREADS > 0) {
    printf("Argument valide : %d threads\n", NUM_THREADS);
} else {
    printf("Conversion fail for %s \n", argv[1]);
    return EXIT_FAILURE;
}
```

### ⚠️ À retenir

`sscanf` retourne :

| Valeur | Signification |
|------|------|
| 1 | conversion réussie |
| 0 | conversion échouée |

---

# 3️⃣ Création de pipe

Les pipes servent à faire communiquer des processus.

Schéma :

    processus A  ---> pipe ---> processus B

Code :

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main() {

    int pipe1[2], pipe2[2];

    // Création des pipes
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Erreur lors de la création des pipes");
        exit(EXIT_FAILURE);
    }
}
```

### ⚠️ À retenir

`pipe()` crée deux descripteurs :

| Index | Rôle |
|------|------|
| pipe[0] | lecture |
| pipe[1] | écriture |

---

# 4️⃣ Création d'un processus avec fork()

`fork()` duplique le processus.

Après le fork :

        Programme
            |
          fork()
            |
        ┌─────────┐
        père     fils

Code :

```c
pid_t pid = fork();

if (pid == 0) {

    // Code du processus fils
    printf("Je suis le fils, PID: %d\n", getpid());

}
```

### ⚠️ Valeur retournée par fork

| Valeur | Signification |
|------|------|
| 0 | processus fils |
| >0 | processus père |
| -1 | erreur |

---

# 5️⃣ Vérifier la fin des processus fils (wait)

Permet au père d'attendre la fin des fils.

```c
for (int i = 1; i < argc; i++) {

    int status;

    pid_t pid = wait(&status);

    if (pid > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {

        printf("convert %s success\n", argv[i]);

        fflush(stdout);
    }
}
```

### ⚠️ À retenir

| Fonction | Rôle |
|------|------|
| wait() | attend un processus fils |
| WIFEXITED(status) | le processus s'est terminé normalement |
| WEXITSTATUS(status) | récupère le code retour |

---

# 6️⃣ Stocker une entrée utilisateur dans un fichier

Fonctions utilisées :

- open()
- write()
- close()

```c
#define OUTPUT_FILE "output.txt"
#define MAX_LENGTH 256

int main() {

    int fd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    char buffer[MAX_LENGTH];

    while (1) {

        printf("Saisir : ");

        if (fgets(buffer, MAX_LENGTH, stdin) == NULL) {
            break;
        }

        write(fd, buffer, strlen(buffer));
    }

    close(fd);
}
```

### ⚠️ Flags open

| Flag | Signification |
|------|------|
| O_WRONLY | écriture |
| O_CREAT | créer fichier |
| O_TRUNC | vider fichier |

---

# 7️⃣ Stocker l'entrée utilisateur dans une variable

Version simple :

```c
#define MAX_LENGTH 256

int main() {

    char stored_text[MAX_LENGTH];

    printf("Saisir : ");

    fgets(stored_text, MAX_LENGTH, stdin);

    printf("\nTexte stocké : %s", stored_text);

    return 0;
}
```

### ⚠️ À retenir

| Fonction | Rôle |
|------|------|
| fgets() | lit une ligne |
| stdin | entrée clavier |

---

# 🧠 Résumé Ultra Partiel

### Arguments programme

```
argc
argv
atoi
sscanf
```

### Processus

```
fork()
wait()
exit()
```

### Communication

```
pipe()
read()
write()
```

### Entrées utilisateur

```
fgets()
sscanf()
```

### Fichiers

```
open()
write()
close()
```
