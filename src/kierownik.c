#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include "podajnik.h"
#include "ipc.h"
#include "logger.h"
#include <pthread.h>
#include <termios.h>

#define LICZBA_KAS 2
#define D_MAX_LICZBA_KLIENTOW 64
#define MAX_KLIENTOW_W_SKLEPIE 10
#define D_PRODUKTOW 10
#define D_CZAS_TRWANIA 6 // w sekundach
#define D_CZAS_PRZED_OTWARCIEM 2 // w sekundach
#define NAME "KIEROWNIK"
int LICZBA_KLIENTOW = MAX_KLIENTOW_W_SKLEPIE;
int ILOSC_PRODUKTOW = D_PRODUKTOW;
int CZAS_TRWANIA = D_CZAS_TRWANIA;
static volatile sig_atomic_t stop_thread = 0;
pthread_t input_thread;

pid_t piekarz_pid;
pid_t kasjer_pid[LICZBA_KAS];
pid_t klient_pid[D_MAX_LICZBA_KLIENTOW]; 

void wyslij_inwentaryzacje() {
    printf("[KIEROWNIK] SYGNAL: INWENTARYZACJA\n");

    sem_wait_mem();
    shm->inwentaryzacja = 1;
    sem_post_mem();

    kill(piekarz_pid, SIGUSR1);
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR1);
}

void wyslij_ewakuacje() {
    printf("[KIEROWNIK] SYGNAL: EWAKUACJA\n");

    sem_wait_mem();
    shm->ewakuacja = 1;
    sem_post_mem();

    kill(piekarz_pid, SIGUSR2);
    
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR2);

    for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++)
        kill(klient_pid[i], SIGUSR2);
    
}


void stworz_klienta() {

    int idx = -1;
    for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++) {
        if (klient_pid[i] == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return; 

    klient_pid[idx] = fork();
    if (klient_pid[idx] == 0) {
        execl("./klient", "klient", NULL);
        perror("Błąd uruchamiania klienta");
        exit(1);
    }
}

void tick() {
    int rand_num = rand() % 1000000;
    if (rand_num == 0){
        stworz_klienta();
    }
}

void menu() {
    printf("Kierownik sklepu\n");
    printf("1. Uruchom symulację\n");
    printf("2. Ustaw czas trwania symulacji (obecny: %d minut)\n", CZAS_TRWANIA);
    printf("Wybierz opcję: ");
    int opt = getchar();
    getchar();
    switch (opt) {
        case '1':
            return;
        case '2':
            printf("Podaj czas trwania symulacji w minutach: ");
            int czas;
            if (scanf("%d", &czas)==EOF)
            {
                printf("Błąd odczytu czasu trwania\n");
                menu();
                break;
            }
            getchar();
            if (czas > 0) {
                CZAS_TRWANIA = czas;
                printf("Czas trwania symulacji ustawiony na %d minut\n", CZAS_TRWANIA);
            } else {
                printf("Nieprawidłowy czas trwania\n");
            }
            menu();
            break;
        default:
            printf("Nieprawidłowa opcja\n");
            menu();
    }
}

void sigchld_handler(int sig) {
    (void)sig;

    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;

        for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++) {
            if (klient_pid[i] == pid) {
                klient_pid[i] = 0;
                
                printf("[KIEROWNIK] Klient %d wyszedl\n", pid);
                break;
            }
        }
    }
}

void *input_thread_func(void *arg) {
    (void)arg;
    while (!stop_thread) {
        int c = getchar();
        if (c == EOF) continue;

        if (c == '1') {
            wyslij_ewakuacje();
        } else if (c == '2') {
            wyslij_inwentaryzacje();
        }

        if (c != '\n') {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        }
    }
    return NULL;
}

static volatile sig_atomic_t stop = 0;

void sigint_handler(int sig) {
    (void)sig;
    stop = 1;
    loguj("KIEROWNIK", "Otrzymano SIGINT, sprzatanie...");
    ipc_cleanup(1);
    exit(0);
}

void cleanup() {
    ipc_cleanup(1);
}


int main() {
    signal(SIGINT, sigint_handler);
    atexit(cleanup);
    printf("[KIEROWNIK] Start procesu\n");
    loguj(NAME, "Start procesu");
    menu();
    char buffer[256];
    sprintf(buffer, "[KIEROWNIK] Symulacja: czas trwania=%d minut\n",
           CZAS_TRWANIA);
    printf("%s", buffer);
    loguj(NAME, buffer);
    //inicjalizacja semaforów i pamięci współdzielonej
    if (ipc_init(1) == -1) {
        fprintf(stderr, "Błąd inicjalizacji IPC\n");
        loguj(NAME, "Błąd inicjalizacji IPC");
        exit(1);
    }

    shm = ipc_get_shm();
    for(int p = 0; p < D_PRODUKTOW; p++){
        podajnik_init_shm(&shm->podajniki[p]);
    }

    if (sem_klientlimit_init(1, LICZBA_KLIENTOW) == -1) {
        fprintf(stderr, "Błąd inicjalizacji semafora limitu klientów\n");
        loguj(NAME, "Błąd inicjalizacji semafora limitu klientów");
        exit(1);
    }
    signal(SIGCHLD, sigchld_handler);

    //uruchomienie piekarza i kasjerów
    piekarz_pid = fork();
    if (piekarz_pid == 0) {
        execl("./piekarz", "piekarz", NULL);
        perror("Błąd uruchamiania piekarza");
        loguj(NAME, "Błąd uruchamiania piekarza");
        ipc_cleanup(1);
        exit(1);
    }
    
    for (int i = 0; i < LICZBA_KAS; i++) {
        kasjer_pid[i] = fork();
        if (kasjer_pid[i] == 0) {
            char id[8];
            sprintf(id, "%d", i+1);
            execl("./kasjer", "kasjer", id, NULL);
            perror("Błąd uruchamiania kasjera");
            ipc_cleanup(1);
            exit(1);
        }
    }
    
    sem_wait_mem();
    shm->kasy_otwarte[0] = 1;
    sem_post_mem();
    
    pthread_create(&input_thread, NULL, input_thread_func, NULL);

    time_t czas_start = time(NULL);
    int godz_koniec = czas_start + CZAS_TRWANIA*1; // w minutach
    int godz_otwarcia = czas_start + D_CZAS_PRZED_OTWARCIEM*1; // w minutach
    while (time(NULL) < godz_koniec) {
        sem_wait_mem();
        int value;
        sem_getvalue(sem_klient, &value);
        if (time(NULL) > godz_otwarcia && shm->sklep_otwarty == 0) {
            shm->sklep_otwarty = 1;
            sprintf(buffer, "[KIEROWNIK] otwieram sklep\n");
            printf("%s", buffer);
            loguj(NAME, buffer);
        }
        if (value <= MAX_KLIENTOW_W_SKLEPIE/2 && shm->kasy_otwarte[1] == 0)
        {
            shm->kasy_otwarte[1] = 1;
            sprintf(buffer, "[KIEROWNIK] Otwieram kasę 2\n");
            printf("%s", buffer);
            loguj(NAME, buffer);
        }
        else if (value > MAX_KLIENTOW_W_SKLEPIE/2 && shm->kasy_otwarte[1] == 1)
        {
            shm->kasy_otwarte[1] = 0;
            sprintf(buffer, "[KIEROWNIK] Zamykam kasę 2\n");
            printf("%s", buffer);
            loguj(NAME, buffer);
        }

        shm->aktualny_czas = time(NULL);
        sem_post_mem();
        tick();
    }
    
    loguj(NAME, "Zamykam piekarnię i kasy");
    sem_wait_mem();
    shm->kasy_otwarte[0] = 0;
    shm->kasy_otwarte[1] = 0;
    shm->sklep_otwarty = 0;
    shm->piekarnia_otwarta = 0;
    sem_post_mem();
    loguj(NAME, "Oczekiwanie na zakończenie procesów potomnych");
    while (wait(NULL) > 0);
    loguj(NAME, "Koniec procesów potomnych");
    
    sprintf(buffer, "\n[KIEROWNIK] Koniec pracy, wprowadź dowolny znak aby wyjść\n");
    printf("%s", buffer);
    sem_wait_logger();
    if (shm->inwentaryzacja)
    {
        loguj(NAME, "[KIEROWNIK] Inwentaryzacja została przeprowadzona");
        raport(NAME, "Podliczanie produktów w podajnikach");
        for (int p = 0; p < ILOSC_PRODUKTOW; p++) {
            int w_podajniku = 0;
            w_podajniku = shm->podajniki[p].count;
            sprintf(buffer, "Produkt %d: w podajniku: %d", p, w_podajniku);
            raport(NAME, buffer);
        }
    }
    sem_post_logger();
    /* ===== SPRZĄTANIE IPC ===== */
    stop_thread = 1;
    pthread_join(input_thread, NULL);
    ipc_cleanup(1);

    sprintf(buffer,"[KIEROWNIK] Zamykanie\n");
    printf("%s", buffer);
    loguj(NAME, buffer);
    return 0;
}
