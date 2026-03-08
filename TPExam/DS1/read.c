#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>

int fin = 0;

void redirect(int signum) {
    if (signum == SIGINT) {
        fin = 1;
    }
}

int main(void)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = redirect;
    sigaction(SIGINT, &act, NULL);

    int output = open("sortie", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (output == -1) {
        perror("open");
        exit(-1);
    }

    char buffer[256];

    while (fin == 0) {
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            write(output, buffer, strlen(buffer));
        }
    }

    close(output);
    return 0;
}