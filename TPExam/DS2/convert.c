#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dijkstra.h"

#define MAX_PROCESSES 8
#define OUTPUT_DIR "sortie"
#define SEM_KEY 1234

typedef struct {
    pid_t pid;
    char *file;
} ProcessInfo;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> <file2> ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int semid = sem_create(SEM_KEY, MAX_PROCESSES);
    if (semid == -1) {
        perror("sem_create");
        return EXIT_FAILURE;
    }

    ProcessInfo infos[argc - 1];
    int launched = 0;

    for (int i = 1; i < argc; i++) {
        P(semid); // attendre une place libre parmi les 8 conversions max

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            V(semid);
            continue;
        }

        if (pid == 0) {
            execlp("mogrify", "mogrify",
                   "-format", "png",
                   "-path", OUTPUT_DIR,
                   argv[i],
                   NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        }

        infos[launched].pid = pid;
        infos[launched].file = argv[i];
        launched++;
    }

    for (int i = 0; i < launched; i++) {
        int status;
        pid_t pid = wait(&status);

        if (pid > 0) {
            for (int j = 0; j < launched; j++) {
                if (infos[j].pid == pid) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        printf("convert %s success\n", infos[j].file);
                    } else {
                        printf("convert %s error\n", infos[j].file);
                    }
                    break;
                }
            }
            V(semid); // une conversion est terminée, on libère une place
        }
    }

    sem_delete(semid);
    return EXIT_SUCCESS;
}