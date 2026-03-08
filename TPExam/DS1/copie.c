#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(void) {

    int id = 1;

    int pid = fork() ;
    if (pid != 0) {
        pid = fork();
        id = 2;
    }

    if (pid == 0)
    {
        printf("Création de Processus %d\n",id) ;
        int input = open("entree" ,O_RDONLY);
        char file[18] = {0};
        sprintf(file,"sortie%d",id);
        int output = open(file, O_CREAT|O_WRONLY|O_TRUNC ,0644);
        char donnee;
        while (read (input, &donnee, sizeof(char)) > 0) {
            write(output, &donnee, sizeof(char)) ;
        }
        printf("Fin du process %d \n", id) ;
        close(input);
        close(output);
    }
    else 
    {
        wait(NULL);
        wait(NULL);
    }
    return 0;
}