#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(void)
{
    int pipe1[2];

    // Création du pipe entre find et xargs
    if (pipe(pipe1) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Processus 1 : find images/ -name "*jpg"
    if (fork() == 0) {
        close(pipe1[0]); // on n'utilise pas la lecture du pipe
        dup2(pipe1[1], STDOUT_FILENO); // stdout -> pipe
        close(pipe1[1]);

        execlp("find", "find", "images/", "-name", "*jpg", NULL);
        perror("execlp find");
        exit(EXIT_FAILURE);
    }

    // Processus 2 : xargs mogrify -resize 16% -path sortie -crop 200x200+0+0
    if (fork() == 0) {
        close(pipe1[1]); // on n'utilise pas l'écriture du pipe
        dup2(pipe1[0], STDIN_FILENO); // stdin <- pipe
        close(pipe1[0]);

        execlp("xargs", "xargs",
               "mogrify",
               "-resize", "16%",
               "-path", "sortie",
               "-crop", "200x200+0+0",
               NULL);
        perror("execlp xargs");
        exit(EXIT_FAILURE);
    }

    // Le parent ferme les deux extrémités du pipe
    close(pipe1[0]);
    close(pipe1[1]);

    // Attendre la fin du pipeline find | xargs mogrify
    wait(NULL);
    wait(NULL);

    // Ensuite : convert sortie/* output.gif
    if (fork() == 0) {
        execlp("sh", "sh", "-c", "convert sortie/* output.gif", NULL);
        perror("execlp convert");
        exit(EXIT_FAILURE);
    }

    wait(NULL);

    return 0;
}