#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#define MAX 100
#define GRID_SIZE 10

void print_usage(void);
int check_winner(int* inputs, int input_size, int* winner, int winner_size);
int get_value(int* draw, int index);

/* Zone à Modifier */

typedef struct {
    int player_id;
    int* grid;
    int found_numbers;
    pthread_mutex_t* lock;
    pthread_barrier_t* barrier;
    int* winner;
    int* called;
} PlayerArgs;

void* player_thread(void* arg) {
    PlayerArgs* args = (PlayerArgs*)arg;
    int player_id = args->player_id;
    int* grid = args->grid;
    pthread_mutex_t* lock = args->lock;
    pthread_barrier_t* barrier = args->barrier;
    int* winner = args->winner;
    int* called = args->called;

    while (*winner == -1) {
        pthread_barrier_wait(barrier);
        if (*winner != -1) break;

        pthread_mutex_lock(lock);
        for (int i = 0; i < GRID_SIZE; i++) {
            if (grid[i] == *called) {
                args->found_numbers++;
                printf("Joueur %d a trouvé un numéro (%d/%d)\n", player_id, args->found_numbers, GRID_SIZE);
                break;
            }
        }
        if (args->found_numbers == GRID_SIZE && *winner == -1) {
            *winner = player_id;
        }
        pthread_mutex_unlock(lock);

        pthread_barrier_wait(barrier);
    }
    pthread_exit(NULL);
}

/* Fin Zone à Modifier */

int main(int argc, char* argv[])
{
    if (argc != 2) {
        print_usage();
        exit(-1);
    }
    int nb_player = 0;
    int check = sscanf(argv[1], "%d", &nb_player);
    if (check != 1 || nb_player <= 0) {
        print_usage();
        exit(-3);
    }

    int draw[MAX] = {0};
    int nb_draw = 0;
    int called = -1;
    int winner = -1;
    int grid[nb_player][GRID_SIZE];


    srand(getpid());
    for(int i = 0; i < nb_player; i++) {
        int nb_value = 0;
        printf("Grid Player n°%d\n", i);
        while(nb_value < GRID_SIZE) {
            int random = rand() % MAX;
            int already_exist = 0;
            for(int j = 0; j < nb_value; j++) {
                if (grid[i][j] == random) {
                    already_exist = 1;
                    break;
                }
            }
            if (!already_exist) {
                grid[i][nb_value] = random;
                nb_value++;
                printf("%d ", random);
            }
        }
        printf("\n\n");
    }

    /* Zone à Modifier */
    pthread_mutex_t lock;
    pthread_barrier_t barrier;
    pthread_t players[nb_player];
    PlayerArgs args[nb_player];
    pthread_mutex_init(&lock, NULL);
    pthread_barrier_init(&barrier, NULL, nb_player + 1);

    for (int i = 0; i < nb_player; i++) {
        args[i].player_id = i;
        args[i].grid = grid[i];
        args[i].lock = &lock;
        args[i].barrier = &barrier;
        args[i].winner = &winner;
        args[i].called = &called;
        args[i].found_numbers = 0;
        pthread_create(&players[i], NULL, player_thread, &args[i]);
    }

    /* Fin Zone à Modifier */

    while (winner == -1 && nb_draw != MAX) {
        /* Zone à Modifier */

        pthread_barrier_wait(&barrier);
        /* Fin Zone à Modifier */
        int random = rand() % (MAX - nb_draw);
        called = get_value(draw, random);
        draw[called] = 1;
        nb_draw++;
        printf("Numéro %d\n", called);
        /* Zone à Modifier */
        pthread_barrier_wait(&barrier);

        /* Fin Zone à Modifier */
    }

    /* Zone à Modifier */
    
    if (winner == -1) {
        winner = 0; // Assurer que winner est valide pour éviter un crash
    }
    
    /* Fin Zone à Modifier */

    if (check_winner(draw, MAX, grid[winner], GRID_SIZE)) {
        printf("Player %d is the Winner!!\n", winner);
    }
    return 0;
}



void print_usage(void)
{
    puts("usage: bingo N (where N>0 is the number of player)");
}

int check_winner(int* inputs, int input_size, int* winner, int winner_size)
{
    assert(winner_size < input_size);

    for (int i = 0; i < winner_size; i++) {
        if (inputs[winner[i]] != 1) {
            return 0;
        }
    }
    return 1;
}

int get_value(int* draw, int index)
{
    int i = -1;
    int pos = 0;
    while (i != index)
    {
        if (draw[pos] == 0)
        {
            i++;
        }
        pos++;
    }
    return pos-1;
}