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
#define LICZBA_KAS 2
#define D_MAX_LICZBA_KLIENTOW 64
#define D_PRODUKTOW 10
#define D_CZAS_TRWANIA 32
#define START_TIME 800  // 8:00 AM
#define NAME "KIEROWNIK"
int LICZBA_KLIENTOW = D_MAX_LICZBA_KLIENTOW;
int ILOSC_PRODUKTOW = D_PRODUKTOW;
int CZAS_TRWANIA = D_CZAS_TRWANIA;
int aktywne_procesy = 0;


pid_t piekarz_pid;
pid_t piekarz_pid_test;
pid_t kasjer_pid[LICZBA_KAS];
pid_t klient_pid[32]; // max 32 klientów


/* =========================== SYGNAŁY =========================== */

void wyslij_inwentaryzacje() {
    printf("[KIEROWNIK] SYGNAL: INWENTARYZACJA\n");

    sem_wait_mem();
    shm->inwentaryzacja = 1;
    sem_post_mem();

    kill(piekarz_pid, SIGUSR1);
    kill(piekarz_pid_test, SIGUSR1);
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR1);
}

void wyslij_ewakuacje() {
    printf("[KIEROWNIK] SYGNAL: EWAKUACJA\n");

    sem_wait_mem();
    shm->ewakuacja = 1;
    sem_post_mem();

    kill(piekarz_pid, SIGUSR2);
    kill(piekarz_pid_test, SIGUSR1);
    /*
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR2);

    for (int i = 0; i < LICZBA_KLIENTOW; i++)
        kill(klient_pid[i], SIGUSR2);
    */
}

/* =========================== FUNKCJE =========================== */

void tick() {
    int rand_num = rand() % 100;
    // tutaj możesz losować zdarzenia w sklepie
}

void stworz_klienta(int indeks) {
    // czekamy jeśli limit klientów osiągnięty
    sem_klientlimit_wait();

    klient_pid[indeks] = fork();
    if (klient_pid[indeks] == 0) {
        execl("./klient", "klient", NULL);
        perror("Błąd uruchamiania klienta");
        exit(1);
    }
    aktywne_procesy++;
}

/* =========================== MENU =========================== */

void menu() {
    printf("Kierownik sklepu\n");
    printf("1. Ustaw czas trwania symulacji w godzinach symulacyjnych (D: 8)\n");
    printf("2. Ustaw pojemność sklepu (D: 10)\n");
    printf("3. Ustaw ilość produktów (D: 10)\n");
    printf("4. Uruchom symulację\n");
    printf("Wybierz opcję: ");
    int opt = getchar();
    getchar(); // zjada \n
    switch (opt) {
        case '1':
            printf("Podaj czas trwania symulacji w godzinach symulacyjnych: ");
            scanf("%d", &CZAS_TRWANIA);
            getchar();
            break;
        case '2':
            printf("Podaj pojemność sklepu: ");
            scanf("%d", &LICZBA_KLIENTOW);
            getchar();
            break;
        case '3':
            printf("Podaj ilość produktów: ");
            scanf("%d", &ILOSC_PRODUKTOW);
            getchar();
            break;
        case '4':
            return;
        default:
            printf("Nieprawidłowa opcja\n");
            menu();
    }
}

/* =========================== MAIN =========================== */

int main() {

    printf("[KIEROWNIK] Start procesu\n");
    loguj(NAME, "Start procesu");
    menu();
    char buffer[256];
    sprintf(buffer, "[KIEROWNIK] Symulacja: czas trwania=%d godzin, pojemność sklepu=%d, ilość produktów=%d\n",
           CZAS_TRWANIA, LICZBA_KLIENTOW, ILOSC_PRODUKTOW);
    printf("%s", buffer);
    loguj(NAME, buffer);
    /* ===== INICJALIZACJA IPC ===== */
    if (ipc_init(1) == -1) {
        fprintf(stderr, "Błąd inicjalizacji IPC\n");
        loguj(NAME, "Błąd inicjalizacji IPC");
        exit(1);
    }

    shm = ipc_get_shm();
    for(int p = 0; p < D_PRODUKTOW; p++){
        podajnik_init_shm(&shm->podajniki[p]);
    }
    /* ===== SEMAFOR LIMITU KLIENTÓW ===== */
    if (sem_klientlimit_init(1, LICZBA_KLIENTOW) == -1) {
        fprintf(stderr, "Błąd inicjalizacji semafora limitu klientów\n");
        loguj(NAME, "Błąd inicjalizacji semafora limitu klientów");
        exit(1);
    }

    produkt_t test_produkt1 = {0, "Rogalik", 5};
    produkt_t test_produkt2 = {0, "Herbatnik", 5};

    podajnik_push_shm(&shm->podajniki[0], &test_produkt1);
    podajnik_push_shm(&shm->podajniki[1], &test_produkt2);

    produkt_t pobrany;
    podajnik_pop_shm(&shm->podajniki[0], &pobrany);
    printf("Pobrano produkt: %s\n", pobrany.name);
    podajnik_pop_shm(&shm->podajniki[1], &pobrany);
    printf("Pobrano produkt: %s\n", pobrany.name);

    
    piekarz_pid = fork();
    if (piekarz_pid == 0) {
        execl("./piekarz", "piekarz", NULL);
        perror("Błąd uruchamiania piekarza");
        loguj(NAME, "Błąd uruchamiania piekarza");
        exit(1);
    }
    aktywne_procesy++;
    piekarz_pid_test = fork();
    if (piekarz_pid_test == 0) {
        execl("./piekarz", "piekarz", NULL);
        perror("Błąd uruchamiania piekarza");
        loguj(NAME, "Błąd uruchamiania piekarza");
        exit(1);
    }
    aktywne_procesy++;

    /* ===== KASJERZY ===== 
    for (int i = 0; i < LICZBA_KAS; i++) {
        kasjer_pid[i] = fork();
        if (kasjer_pid[i] == 0) {
            char id[8];
            sprintf(id, "%d", i);
            execl("./kasjer", "kasjer", id, NULL);
            perror("Błąd uruchamiania kasjera");
            exit(1);
        }
        aktywne_procesy++;
    }
    */

    
    /* ===== SYMULACJA CZASU ===== */
    int czas_symulacji = CZAS_TRWANIA * 10; // w 10 minutach
    for (int t = 0; t < czas_symulacji; ++t) {
        //usleep(100000); // 0.1 sekundy = 1 minuta symulacyjna
        //loguj(NAME, "wait mem");
        sem_wait_mem();
        shm->aktualna_liczba_procesow = aktywne_procesy;
        shm->aktualny_czas = START_TIME + t;
        sem_post_mem();
        sprintf(buffer, "[KIEROWNIK] Czas symulacji: %02d:%02d\n",
               (shm->aktualny_czas) / 60, (shm->aktualny_czas) % 60);
        printf("%s", buffer);
        loguj(NAME, buffer);
        // START TURY
        //loguj(NAME, "semtickstart postpre");
        for(int i=0; i<aktywne_procesy; i++){
            sem_tick_start_post();
           // loguj(NAME, "semtickstart post");
        }

        // CZEKAJ NA ZAKOŃCZENIE TURY
        for(int i=0; i<aktywne_procesy; i++){
            sem_tick_done_wait();
           // loguj(NAME, "semtick done wait");
        }
        tick();
    }
    /*
    sleep(2);
    wyslij_inwentaryzacje();
    sleep(2);
    */
    //sleep(0);
    //wyslij_ewakuacje();
    
    //loguj(NAME, "wait mem close shop");
    sem_wait_mem();
    shm->sklep_otwarty = 0;
    sem_post_mem();
    //loguj(NAME, "end mem close shop");
    /* ===== CZEKAJ NA DZIECI ===== */
    for (int i = 0; i < aktywne_procesy; i++) 
    {
    sem_tick_start_post();
    }
    for (int i = 0; i < 1 + LICZBA_KAS + LICZBA_KLIENTOW; i++)
    {
        //loguj(NAME, "waiting for child");
        wait(NULL);
        //loguj(NAME, "child ended");
    }
    /* ===== RAPORT KOŃCOWY ===== */
    
    sprintf(buffer, "\n[KIEROWNIK] RAPORT KOŃCOWY\n");
    printf("%s", buffer);
    loguj(NAME, buffer);
    for (int p = 0; p < ILOSC_PRODUKTOW; p++) {
        int sprzedano = 0;
        for (int k = 0; k < LICZBA_KAS; k++)
            sprzedano += shm->sprzedane[k][p];

        sprintf(buffer, "Produkt %d: wyprodukowano=%d sprzedano=%d\n",
               p, shm->wyprodukowane[p], sprzedano);
        printf("%s", buffer);
        loguj(NAME, buffer);
    }

    /* ===== SPRZĄTANIE IPC ===== */
    ipc_cleanup(1);

    sprintf(buffer,"[KIEROWNIK] Koniec\n");
    printf("%s", buffer);
    loguj(NAME, buffer);
    return 0;
}
