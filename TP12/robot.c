// robot.c
// Version concurrente (pthread) + affichage ANSI
// Corrections:
// 1) Démarrage toujours au même endroit (rx/ry/rdir forcés dans init_world)
// 2) Anti-oscillation gauche/droite : IR tourne vers le côté où il y a le plus d'espace + persistance avoid_ticks
// 3) Anti-boucle "reculer" infinie : si contact et commande=RECULER => on force une rotation quelques cycles
// 4) Plus de warning usleep : nanosleep() (C11 propre)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>   // getchar()
#include <time.h>     // nanosleep()

/* ========= AFFICHAGE ANSI ========= */
#define CLEARSCR "\x1B[2J\x1B[;H"

/* ========= PARAMETRES ========= */
/* Périodes (en millisecondes) */
#define X_MS 150   /* Contrôleur */
#define A_MS 120   /* Ecran */
#define S_MS 120   /* IR */
#define K_MS 120   /* US */
#define Z_MS 30    /* Bumper (petit) */

/* Seuil distance "d" (en cases) */
#define D_THRESHOLD 2

/* Grille du monde */
#define H 18
#define W 50

/* ========= TYPES ========= */
typedef enum { NORD=0, EST=1, SUD=2, OUEST=3 } Dir;

typedef enum {
    CMD_AVANCER=0,
    CMD_RECULER,
    CMD_GAUCHE,
    CMD_DROITE
} Cmd;

static const char* cmd_to_str(Cmd c) {
    switch(c){
        case CMD_AVANCER: return "avancer";
        case CMD_RECULER: return "reculer";
        case CMD_GAUCHE:  return "a gauche";
        case CMD_DROITE:  return "a droite";
        default: return "?";
    }
}

/* ========= ETAT PARTAGE ========= */

/* Mémoire écran (énoncé : mem_Cmd + mem_Flag) */
static pthread_mutex_t m_mem = PTHREAD_MUTEX_INITIALIZER;
static Cmd  mem_Cmd = CMD_AVANCER;
static int  mem_Flag = 0;

/* Etats capteurs (flags + cmd proposées) */
static pthread_mutex_t m_sens = PTHREAD_MUTEX_INITIALIZER;
static int Drapeau_IR = 0;  static Cmd Cmd_IR = CMD_AVANCER;
static int Drapeau_US = 0;  static Cmd Cmd_US = CMD_AVANCER;
static int Drapeau_BU = 0;  static Cmd Cmd_BU = CMD_AVANCER;

/* Monde + robot */
static pthread_mutex_t m_world = PTHREAD_MUTEX_INITIALIZER;
static char world[H][W+1];

static int rx=2, ry=2;     /* position robot */
static Dir rdir=EST;       /* direction robot */
static int bumper_contact = 0;  /* 1 si collision récente */
static volatile int running = 1;

/* Anti-oscillation (mémoire d'évitement) */
static int avoid_ticks = 0;      /* cycles restants */
static Cmd avoid_cmd = CMD_AVANCER;

/* ========= OUTILS ========= */
static void msleep(int ms){
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void dir_front(Dir d, int *dx, int *dy){
    *dx=0; *dy=0;
    if(d==NORD) *dy=-1;
    else if(d==EST) *dx=1;
    else if(d==SUD) *dy=1;
    else if(d==OUEST) *dx=-1;
}

static void dir_left(Dir d, int *dx, int *dy){
    Dir ld = (Dir)((d+3)%4);
    dir_front(ld, dx, dy);
}
static void dir_right(Dir d, int *dx, int *dy){
    Dir rd = (Dir)((d+1)%4);
    dir_front(rd, dx, dy);
}

/* Distance en cases jusqu'au prochain mur */
static int distance_to_wall(int x, int y, int dx, int dy){
    int dist=0;
    while(1){
        x += dx; y += dy;
        dist++;
        if(x<0 || x>=W || y<0 || y>=H) return dist;
        if(world[y][x]=='#') return dist;
    }
}

/* Appliquer commande (avance/recul/tourne) + bumper */
static void apply_command(Cmd c){
    int fdx,fdy;
    dir_front(rdir,&fdx,&fdy);

    world[ry][rx]='.';
    bumper_contact = 0;

    if(c==CMD_GAUCHE){
        rdir = (Dir)((rdir+3)%4);
    } else if(c==CMD_DROITE){
        rdir = (Dir)((rdir+1)%4);
    } else if(c==CMD_AVANCER){
        int nx = rx + fdx;
        int ny = ry + fdy;
        if(world[ny][nx]=='#'){
            bumper_contact = 1;
        } else {
            rx = nx; ry = ny;
        }
    } else if(c==CMD_RECULER){
        int nx = rx - fdx;
        int ny = ry - fdy;
        if(world[ny][nx]=='#'){
            bumper_contact = 1;
        } else {
            rx = nx; ry = ny;
        }
    }

    world[ry][rx]='R';
}

static void init_world(void){
    /* Force l'état initial (empêche le robot d'apparaître ailleurs) */
    rx = 2; ry = 2;
    rdir = EST;
    bumper_contact = 0;
    avoid_ticks = 0;
    avoid_cmd = CMD_AVANCER;

    /* Tout vide */
    for(int y=0;y<H;y++){
        for(int x=0;x<W;x++) world[y][x]='.';
        world[y][W]='\0';
    }
    /* Bordures */
    for(int x=0;x<W;x++){ world[0][x]='#'; world[H-1][x]='#'; }
    for(int y=0;y<H;y++){ world[y][0]='#'; world[y][W-1]='#'; }

    /* Obstacles internes (exemple) */
    for(int x=10;x<40;x++) world[6][x]='#';
    for(int y=9;y<15;y++) world[y][20]='#';
    for(int x=25;x<45;x++) world[12][x]='#';

    /* Robot */
    world[ry][rx]='R';
}

/* ========= THREADS ========= */

static void* t_infrarouge(void* arg){
    (void)arg;
    while(running){
        pthread_mutex_lock(&m_world);

        int ldx,ldy, rdx,rdy;
        dir_left(rdir,&ldx,&ldy);
        dir_right(rdir,&rdx,&rdy);

        int Dg = distance_to_wall(rx,ry,ldx,ldy);
        int Dd = distance_to_wall(rx,ry,rdx,rdy);

        pthread_mutex_unlock(&m_world);

        pthread_mutex_lock(&m_sens);

        if(Dg < D_THRESHOLD || Dd < D_THRESHOLD){
            Drapeau_IR = 1;

            if(Dg < D_THRESHOLD && Dd < D_THRESHOLD){
                Cmd_IR = CMD_RECULER;
            } else {
                /* Tourner vers le côté avec PLUS d'espace (évite ping-pong) */
                if (Dg < Dd) Cmd_IR = CMD_DROITE;  // mur plus proche à gauche
                else         Cmd_IR = CMD_GAUCHE;  // mur plus proche à droite
            }
        } else {
            Drapeau_IR = 0;
        }

        pthread_mutex_unlock(&m_sens);

        msleep(S_MS);
    }
    return NULL;
}

static void* t_ultrason(void* arg){
    (void)arg;
    while(running){
        pthread_mutex_lock(&m_world);

        int fdx,fdy;
        dir_front(rdir,&fdx,&fdy);
        int D = distance_to_wall(rx,ry,fdx,fdy);

        pthread_mutex_unlock(&m_world);

        pthread_mutex_lock(&m_sens);
        if(D < D_THRESHOLD){
            Drapeau_US = 1;
            Cmd_US = CMD_RECULER;
        } else {
            Drapeau_US = 0;
        }
        pthread_mutex_unlock(&m_sens);

        msleep(K_MS);
    }
    return NULL;
}

static void* t_bumper(void* arg){
    (void)arg;
    while(running){
        pthread_mutex_lock(&m_world);
        int contact = bumper_contact;
        pthread_mutex_unlock(&m_world);

        pthread_mutex_lock(&m_sens);
        if(contact==1){
            Drapeau_BU = 1;
            Cmd_BU = CMD_RECULER;
        } else {
            Drapeau_BU = 0;
        }
        pthread_mutex_unlock(&m_sens);

        msleep(Z_MS);
    }
    return NULL;
}

static void* t_controleur(void* arg){
    (void)arg;
    while(running){
        Cmd commande = CMD_AVANCER;
        int drapeau = 0;

        /* 1) Mode évitement : persistance */
        if (avoid_ticks > 0) {
            commande = avoid_cmd;
            drapeau = 1;
            avoid_ticks--;
        } else {
            /* 2) Lecture capteurs (priorité énoncé) */
            pthread_mutex_lock(&m_sens);

            int ir = Drapeau_IR; Cmd cir = Cmd_IR;
            int us = Drapeau_US; Cmd cus = Cmd_US;
            int bu = Drapeau_BU; Cmd cbu = Cmd_BU;

            pthread_mutex_unlock(&m_sens);

            if(ir){ commande = cir; drapeau = 1; }
            if(us){ commande = cus; drapeau = 1; }
            if(bu){ commande = cbu; drapeau = 1; }

            /* 3) Si un capteur déclenche, on fige qq cycles */
            if (drapeau) {
                if (commande == CMD_RECULER) {
                    avoid_cmd = CMD_RECULER;
                    avoid_ticks = 2;
                } else if (commande == CMD_GAUCHE || commande == CMD_DROITE) {
                    avoid_cmd = commande;
                    avoid_ticks = 3;
                }
            }
        }

        /* ---- Anti-blocage reculer infini ----
           Si on était déjà en contact (choc) et qu'on insiste sur RECULER,
           on tourne pour se dégager. */
        pthread_mutex_lock(&m_world);
        int contact = bumper_contact;
        pthread_mutex_unlock(&m_world);

        if (contact == 1 && commande == CMD_RECULER) {
            commande = CMD_DROITE;   /* choix simple : toujours à droite */
            avoid_cmd = commande;
            avoid_ticks = 4;
            drapeau = 1;
        }

        /* Appliquer commande aux servos */
        pthread_mutex_lock(&m_world);
        apply_command(commande);
        pthread_mutex_unlock(&m_world);

        /* Mise à jour mémoire écran */
        pthread_mutex_lock(&m_mem);
        mem_Cmd  = commande;
        mem_Flag = drapeau;
        pthread_mutex_unlock(&m_mem);

        msleep(X_MS);
    }
    return NULL;
}

static void* t_ecran(void* arg){
    (void)arg;
    while(running){
        /* Copier mem_Cmd/mem_Flag */
        pthread_mutex_lock(&m_mem);
        Cmd C = mem_Cmd;
        int F = mem_Flag;
        pthread_mutex_unlock(&m_mem);

        /* Copier états capteurs */
        pthread_mutex_lock(&m_sens);
        int ir = Drapeau_IR, us = Drapeau_US, bu = Drapeau_BU;
        Cmd cir = Cmd_IR, cus = Cmd_US, cbu = Cmd_BU;
        pthread_mutex_unlock(&m_sens);

        /* Snapshot monde */
        pthread_mutex_lock(&m_world);
        char snap[H][W+1];
        for(int y=0;y<H;y++) strcpy(snap[y], world[y]);
        int x=rx,y=ry; Dir d=rdir; int contact=bumper_contact;
        pthread_mutex_unlock(&m_world);

        printf("%s", CLEARSCR);
        printf("ROBOT (concurrent) - appuie sur q pour quitter\n");
        printf("Pos=(%d,%d) Dir=%s  BumperContact=%d\n",
               x,y, (d==NORD?"NORD":d==EST?"EST":d==SUD?"SUD":"OUEST"), contact);

        printf("mem_Cmd=%s | mem_Flag=%d | avoid_ticks=%d (%s)\n",
               cmd_to_str(C), F, avoid_ticks, cmd_to_str(avoid_cmd));

        printf("IR: flag=%d cmd=%s | US: flag=%d cmd=%s | BU: flag=%d cmd=%s\n",
               ir, cmd_to_str(cir), us, cmd_to_str(cus), bu, cmd_to_str(cbu));

        printf("\n");
        for(int yy=0;yy<H;yy++){
            printf("%s\n", snap[yy]);
        }
        fflush(stdout);

        msleep(A_MS);
    }
    return NULL;
}

/* Thread clavier : q pour quitter */
static void* t_keyboard(void* arg){
    (void)arg;
    while(running){
        int c = getchar();
        if(c=='q' || c=='Q'){
            running = 0;
            break;
        }
    }
    return NULL;
}

/* ========= MAIN ========= */
int main(void){
    setvbuf(stdout, NULL, _IONBF, 0);

    pthread_t th_ir, th_us, th_bu, th_ctrl, th_scr, th_kb;

    pthread_mutex_lock(&m_world);
    init_world();
    pthread_mutex_unlock(&m_world);

    pthread_create(&th_ir,   NULL, t_infrarouge, NULL);
    pthread_create(&th_us,   NULL, t_ultrason,   NULL);
    pthread_create(&th_bu,   NULL, t_bumper,     NULL);
    pthread_create(&th_ctrl, NULL, t_controleur, NULL);
    pthread_create(&th_scr,  NULL, t_ecran,      NULL);
    pthread_create(&th_kb,   NULL, t_keyboard,   NULL);

    pthread_join(th_kb, NULL);
    running = 0;

    pthread_join(th_ir, NULL);
    pthread_join(th_us, NULL);
    pthread_join(th_bu, NULL);
    pthread_join(th_ctrl,NULL);
    pthread_join(th_scr, NULL);

    printf("\nFin.\n");
    return 0;
}