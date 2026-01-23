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

#define LICZBA_KAS 2 // stała liczba kas
#define D_MAX_LICZBA_KLIENTOW 5000 // maksymalna liczba klientów w systemie
#define MAX_KLIENTOW_W_SKLEPIE 100 // maksymalna liczba klientów w sklepie jednocześnie
#define D_PRODUKTOW 10 // liczba różnych produktów
#define D_CZAS_TRWANIA 60 // w sekundach
#define D_CZAS_PRZED_OTWARCIEM 2 // w sekundach
#define NAME "KIEROWNIK" // nazwa procesu do logów


int LICZBA_KLIENTOW = MAX_KLIENTOW_W_SKLEPIE;
int ILOSC_PRODUKTOW = D_PRODUKTOW;
int CZAS_TRWANIA = D_CZAS_TRWANIA;

static volatile sig_atomic_t stop_thread = 0; // flaga do zatrzymania wątku input
pthread_t input_thread;

pid_t piekarz_pid; // PID piekarza
pid_t kasjer_pid[LICZBA_KAS]; // PID kasjerów
pid_t klient_pid[D_MAX_LICZBA_KLIENTOW]; // PID klientów

// rodzaj testu
int test_mode = 0;


/*wysyła sygnał do piekarza i kasjerów o inwentaryzacji*/
void wyslij_inwentaryzacje() {
    printf("[KIEROWNIK] SYGNAL: INWENTARYZACJA\n");

    sem_wait_mem();
    shm->inwentaryzacja = 1;
    sem_post_mem();
    if (test_mode != 1){
    kill(piekarz_pid, SIGUSR1);
    }
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR1);
}
/*wysyła sygnał do piekarza, kasjerów i klientów o ewakuacji*/
void wyslij_ewakuacje() {
    printf("[KIEROWNIK] SYGNAL: EWAKUACJA\n");

    sem_wait_mem();
    shm->ewakuacja = 1;
    sem_post_mem();
    if (test_mode != 1){
        kill(piekarz_pid, SIGUSR2);
    }
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR2);

    for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++)
        kill(klient_pid[i], SIGUSR2);
    
}

/*tworzy nowego klienta jeśli jest miejsce w tablicy klient_pid*/
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
// licznik klientów utworzonych w trybie tesowtowym 3
int klienci_total = 0;
/*wyświetla menu i obsługuje wybór użytkownika*/
void menu() {
    printf("Kierownik sklepu\n");
    printf("1. Uruchom symulację\n");
    printf("2. Ustaw czas trwania symulacji (obecny: %d minut)\n", CZAS_TRWANIA);
    printf("3. Uruchom test 1 (bez piekarza)\n");
    printf("4. Uruchom test 2 (podniesienie semafora klientow do maks)\n");
    printf("5. Uruchom test 3 (Spam klientow)\n");
    printf("6. Uruchom test 4 (bez klientów)\n");
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
        case '3':
            test_mode = 1;
            printf("Tryb testowy 1 włączony (bez piekarza)\nOczekiwany rezultat: 0 produktów wyprodukowanych\n");
            return;
        case '4':
            test_mode = 2;
            printf("Tryb testowy 2 włączony (zapchany semafor klientów)\nOczekiwany rezultat: Klienci nie wchodza do sklepu\n");
            return;
        case '5':
            test_mode = 3;
            printf("Tryb testowy 3 włączony (spam klientów)\nOczekiwany rezultat: Brak zawieszenia programu\n");
            return;
        case '6':
            test_mode = 4;
            printf("Tryb testowy 4 włączony (brak klientów)\nOczekiwany rezultat: 0 produktów sprzedanych, zabranych\n");
            return;
        default:
            printf("Nieprawidłowa opcja\n");
            menu();
    }
}
/*obsługuje zakończenie procesów klientów*/
void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    // sprzątaj WSZYSTKIE zakończone dzieci
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        // usuń z tablicy klientów
        for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++) {
            if (klient_pid[i] == pid) {
                klient_pid[i] = 0;
                break;
            }
        }

        printf("[KIEROWNIK] Posprzątano proces %d\n", pid);
    }
}

static volatile sig_atomic_t ewakuacja = 0;

void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    for (int i = 0; i < D_MAX_LICZBA_KLIENTOW; i++) {
        sem_klientlimit_post();
    }
}

/*funkcja wątku obsługującego wejście użytkownika*/
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

/*obsługa sygnału SIGINT do sprzątania i zakończenia*/
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

/* GŁÓWNA FUNKCJA KIEROWNIKA */
int main() {
    //obsługa sygnału SIGINT
    signal(SIGINT, sigint_handler);
    atexit(cleanup);

    //obsługa ewakuacji
    struct sigaction sa;
    sa.sa_handler = sig_ewakuacja;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR2, &sa, NULL);

    //obsługa sygnału SIGCHLD do czyszczenia zakończonych procesów klientów
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    printf("[KIEROWNIK] Start procesu\n");
    loguj(NAME, "Start procesu");

    //wyświetlenie menu
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
    //inicjalizacja semafora limitu klientów w sklepie
    if (sem_klientlimit_init(1, LICZBA_KLIENTOW) == -1) {
        fprintf(stderr, "Błąd inicjalizacji semafora limitu klientów\n");
        loguj(NAME, "Błąd inicjalizacji semafora limitu klientów");
        exit(1);
    }

    //uruchomienie piekarza i kasjerów
    if (test_mode != 1){
        piekarz_pid = fork();
        if (piekarz_pid == 0) {
            execl("./piekarz", "piekarz", NULL);
            perror("Błąd uruchamiania piekarza");
            loguj(NAME, "Błąd uruchamiania piekarza");
            ipc_cleanup(1);
            exit(1);
        }
    }
    printf("[KIEROWNIK] Utworzono piekarza\n");
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
        printf("[KIEROWNIK] Utworzono kasjera: %d\n", i);
    }
    
    //obsługa trybu testowego 2/ maksymalna liczba klientów w sklepie
    if (test_mode == 2) {
        for (int i = 0; i < MAX_KLIENTOW_W_SKLEPIE; i++) {
            sem_klientlimit_wait(); // Zwiększ limit klientów do maksimum (żaden klient nie powinien wejść)
        }
    }

    //otwarcie pierwszej kasy
    sem_wait_mem();
    shm->kasy_otwarte[0] = 1;
    shm->w_kolejce = 0;
    sem_post_mem();
    
    //uruchomienie wątku obsługującego wejście użytkownika
    pthread_create(&input_thread, NULL, input_thread_func, NULL);

    //obliczenia godziny zakończenia i otwarcia sklepu
    time_t czas_start = time(NULL);
    int godz_koniec = czas_start + CZAS_TRWANIA*1; // w minutach
    int godz_otwarcia = czas_start + D_CZAS_PRZED_OTWARCIEM*1; // w minutach
    
    /*GŁÓWNA PĘTLA KIEROWNIKA*/
    while (time(NULL) < godz_koniec) {
        if (ewakuacja) {
            loguj(NAME, "Otrzymano sygnal ewakuacji");
            break;
        }
        //otwarcie semafora pamięci współdzielonej
        sem_wait_mem();
        //pobieranie ilosci klientów w sklepie
        int value;
        sem_getvalue(sem_klient, &value);
        //zarządzanie otwarciem sklepu i kas
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
        //aktualizacja czasu w pamięci współdzielonej
        shm->aktualny_czas = time(NULL);
        //opuszczenie semafora pamięci współdzielonej
        sem_post_mem();
        
        //tworzenie klientów
        if (shm->sklep_otwarty == 1)
        {
            int rand_num = rand() % 1000000;
            if ((rand_num == 0 || test_mode == 3) && test_mode!=4){
                stworz_klienta();
                klienci_total++;
                printf("[KIEROWNIK] Utworzono klienta: %d\n", klienci_total);
            }

        }
    }
    // zamknięcie piekarni i kas
    loguj(NAME, "Zamykam piekarnię i kasy");
    sem_wait_mem();
    shm->kasy_otwarte[0] = 0;
    shm->kasy_otwarte[1] = 0;
    shm->sklep_otwarty = 0;
    shm->piekarnia_otwarta = 0;
    sem_post_mem();
    // w trybie testowym 2 czyszczenie semafora limitu klientów
    if (test_mode == 2) {
        for (int i = 0; i < MAX_KLIENTOW_W_SKLEPIE; i++) {
            sem_klientlimit_post(); //czyszczenie semafora
        }
    }

    // oczekiwanie na zakończenie procesów potomnych
    loguj(NAME, "Oczekiwanie na zakończenie procesów potomnych");
    while (wait(NULL) > 0);
    loguj(NAME, "Koniec procesów potomnych");
    
    // oczekiwanie na zakończenie wątku wejścia
    sprintf(buffer, "\n[KIEROWNIK] Koniec pracy, wprowadź dowolny znak aby wyjść\n");
    printf("%s", buffer);

    // raport z inwentaryzacji jeśli została przeprowadzona
    if (shm->inwentaryzacja)
    {
        sem_wait_logger();
        loguj(NAME, "[KIEROWNIK] Inwentaryzacja została przeprowadzona");
        raport(NAME, "Podliczanie produktów w podajnikach");
        for (int p = 0; p < ILOSC_PRODUKTOW; p++) {
            int w_podajniku = 0;
            w_podajniku = shm->podajniki[p].count;
            sprintf(buffer, "Produkt %d: w podajniku: %d", p, w_podajniku);
            raport(NAME, buffer);
        }
        sem_post_logger();
    }
    // sprzątanie i zakończenie wątku
    stop_thread = 1;
    pthread_join(input_thread, NULL);
    ipc_cleanup(1);

    sprintf(buffer,"[KIEROWNIK] Zamykanie\n");
    printf("%s", buffer);
    loguj(NAME, buffer);

    return 0;
}
