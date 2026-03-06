## Création/Récupération

```c
int shmget(key_t key, size_t size, int shmflg);
```

key : identifiant du segment.
size : taille de la mémoire partagée.
shmflg : droits d'accès.



## Attachement/Détachement :

```c
void *shmat(int shmid, const void *_Nullable shmaddr, int shmflg);
int shmdt(const void *shmaddr);
```

shmat : Attache le segment mémoire et retourne un pointeur.
shmdt : Détache le segment mémoire.



## Suppression :
```c
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```
IPC_RMID : Supprime la mémoire partagée.