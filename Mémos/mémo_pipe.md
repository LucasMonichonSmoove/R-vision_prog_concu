# Mémo -- Pipes, Redirections et Processus (C / Unix)

## Constantes importantes (open)

  Constante   Signification
  ----------- ------------------------------------
  O_RDONLY    Lecture seule
  O_WRONLY    Écriture seule
  O_RDWR      Lecture et écriture
  O_CREAT     Créer le fichier s'il n'existe pas
  O_APPEND    Écrire à la fin du fichier
  O_TRUNC     Vider le fichier avant écriture

Compilation :

``` bash
gcc -Wall -Wextra -g test2.c -o test2
./test2
```

------------------------------------------------------------------------

# Redirections de base

## commande \> fichier

### echo "Bonjour" \> message.txt

``` c
int fd = open("message.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);
execlp("echo", "echo", "Bonjour", NULL);
```

------------------------------------------------------------------------

### echo "Nouvelle ligne" \>\> log.txt

``` c
int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);
execlp("echo", "echo", "Nouvelle ligne", NULL);
```

------------------------------------------------------------------------

### sort \< data.txt

``` c
int fd = open("data.txt", O_RDONLY);
dup2(fd, STDIN_FILENO);
close(fd);
execlp("sort", "sort", NULL);
```

------------------------------------------------------------------------

# Exemple Pipeline

Commande shell :

``` bash
grep "invalid credentials" < server.log | cut -d, -f3 | sort > sortie
```

Principe :

1.  grep lit server.log
2.  résultat envoyé dans pipe1
3.  cut lit pipe1 et écrit pipe2
4.  sort lit pipe2 et écrit dans sortie

Structure générale :

``` c
pipe(pipe1);
pipe(pipe2);

fork(); // grep
fork(); // cut
fork(); // sort

dup2(...) pour rediriger stdin / stdout
execlp(...) pour lancer la commande
```

Le processus parent ferme les pipes et fait :

``` c
wait(NULL);
wait(NULL);
wait(NULL);
```

------------------------------------------------------------------------

# Opérateur logique \|\|

Commande shell :

``` bash
ls /mon_dossier || mkdir /mon_dossier
```

Logique :

1.  exécuter ls
2.  attendre la fin
3.  si code retour ≠ 0 → exécuter mkdir

Principe C :

``` c
wait(&status);

if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    printf("Le dossier existe\n");
} else {
    execlp("mkdir","mkdir","/mon_dossier",NULL);
}
```

------------------------------------------------------------------------

# Pipeline conditionnel

Commande :

``` bash
ls /dossier_existant || echo "Le dossier n'existe pas" | tr a-z A-Z
```

Logique :

1.  tester ls
2.  si échec :
    -   echo
    -   pipe vers tr
    -   tr transforme en majuscules

Schéma :

    echo → pipe → tr

------------------------------------------------------------------------

# Pipeline complexe

Commande :

``` bash
grep "ERROR" < file.log | awk '{print $1}' | sort | uniq -c
```

Étapes :

1.  grep lit file.log
2.  awk extrait la première colonne
3.  sort trie
4.  uniq compte les occurrences

Structure :

    file.log
       ↓
    grep
       ↓ pipe1
    awk
       ↓ pipe2
    sort
       ↓ pipe3
    uniq -c

Puis archive :

``` bash
tar -czf logs.tar.gz file.log
```

------------------------------------------------------------------------

# Commande avec &&

``` bash
mkdir test && echo "Dossier créé"
```

Logique :

1.  créer dossier
2.  si succès → echo

Principe C :

``` c
wait(&status);

if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    execlp("echo","echo","Dossier créé",NULL);
}
```

------------------------------------------------------------------------

# Pipeline avec transformation

Commande :

``` bash
cut -f 1,3 -d : < /etc/passwd | sed 's+^\(.*\):\(.*\)+\2:\1+' | sort -n > users
```

Étapes :

1.  cut extrait colonnes 1 et 3
2.  sed inverse les champs
3.  sort trie numériquement
4.  résultat dans fichier users

Architecture :

    /etc/passwd
         ↓
    cut
         ↓ pipe1
    sed
         ↓ pipe2
    sort
         ↓
    users

------------------------------------------------------------------------

# Rappel Architecture Pipeline en C

Toujours suivre ces étapes :

1️⃣ créer les pipes

``` c
pipe(pipe1);
pipe(pipe2);
```

2️⃣ fork pour chaque commande

``` c
fork();
```

3️⃣ rediriger les entrées/sorties

``` c
dup2(pipe1[0], STDIN_FILENO);
dup2(pipe1[1], STDOUT_FILENO);
```

4️⃣ fermer les pipes inutiles

``` c
close(pipe1[0]);
close(pipe1[1]);
```

5️⃣ lancer la commande

``` c
execlp("grep","grep","ERROR",NULL);
```

6️⃣ attendre les processus

``` c
wait(NULL);
```

------------------------------------------------------------------------

# Résumé ultra important pour le partiel

À retenir absolument :

  Concept              Fonction
  -------------------- ----------
  Processus            fork()
  Attendre             wait()
  Exécuter programme   execlp()
  Redirection          dup2()
  Communication        pipe()
  Fichiers             open()
  Fermer               close()

Architecture classique pipeline :

    commande1 → pipe → commande2 → pipe → commande3

Toujours :

1.  fork
2.  dup2
3.  close
4.  execlp
