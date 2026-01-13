#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#include "ipc.h"
#include "logger.h"

#define LICZBA_KAS 2
#define D_MAX_LICZBA_KLIENTOW 10
#define D_PRODUKTOW 10
#define D_CZAS_TRWANIA 8
#define START_TIME 800  // 8:00 AM

int LICZBA_KLIENTOW = D_MAX_LICZBA_KLIENTOW;
int ILOSC_PRODUKTOW = D_PRODUKTOW;
int CZAS_TRWANIA = D_CZAS_TRWANIA;


pid_t piekarz_pid;
pid_t kasjer_pid[LICZBA_KAS];
pid_t klient_pid[32]; // max 32 klientów

shm_data_t *shm;
sem_t *sem;

sem_t *klient_sem;

void tick() {
    int rand_num = rand() % 100;
}


void stworz_klienta() {
    klient_pid[LICZBA_KLIENTOW++] = fork();
    if (klient_pid[LICZBA_KLIENTOW - 1] == 0) {
        execl("./klient", "klient", NULL);
        perror("Bład uruchamiania klientów");
        exit(1);
    }
    //sem_wait(klient_sem); przesunac na klienta

}

/* ================= SYGNALY ================= */

void wyslij_inwentaryzacje() {
    printf("[KIEROWNIK] SYGNAL: INWENTARYZACJA\n");

    sem_wait(sem);
    shm->inwentaryzacja = 1;
    sem_post(sem);

    kill(piekarz_pid, SIGUSR1);
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR1);
}

void wyslij_ewakuacje() {
    printf("[KIEROWNIK] SYGNAL: EWAKUACJA\n");

    sem_wait(sem);
    shm->ewakuacja = 1;
    sem_post(sem);

    kill(piekarz_pid, SIGUSR2);
    for (int i = 0; i < LICZBA_KAS; i++)
        kill(kasjer_pid[i], SIGUSR2);

    for (int i = 0; i < LICZBA_KLIENTOW; i++)
        kill(klient_pid[i], SIGUSR2);
}

/* ================= MAIN ================= */

void menu() {
    printf("Kierownik sklepu\n");
    printf("1. Ustaw czas trwania symulacji w godzinach symulacyjych (D: 8)\n");
    printf("2. Ustaw pojemność sklepu (D: 10)\n");
    printf("3. Ustaw ilość produktów (D: 10)\n");
    printf("4. Uruchom symulację\n");
    printf("Wybierz opcje: ");
    switch (getchar()) {
        case '1':
            printf("Podaj czas trwania symulacji w godzinach symulacyjych: ");
            scanf("%d", &CZAS_TRWANIA);
            break;
        case '2':
            printf("Podaj pojemność sklepu: ");
            scanf("%d", &LICZBA_KLIENTOW);
            break;
        case '3':
            printf("Podaj ilość produktów: ");
            scanf("%d", &ILOSC_PRODUKTOW);
            break;
        case '4':
            return;
        default:
            printf("Nieprawidłowa opcja\n");
    }
}


int main() {

    printf("[KIEROWNIK] Start procesu\n");
    menu();
    printf("[KIEROWNIK] Symulacja: czas trwania=%d godzin, pojemność sklepu=%d, ilość produktów=%d\n",
           CZAS_TRWANIA, LICZBA_KLIENTOW, ILOSC_PRODUKTOW);

    
    if (ipc_init(1) == -1) {
        fprintf(stderr, "Blad inicjalizacji IPC\n");
        exit(1);
    }

    shm = ipc_get_shm();
    sem = ipc_get_sem();
    klient_sem = sem_klientlimit_get();
    /* ===== PIEKARZ ===== 
    piekarz_pid = fork();
    if (piekarz_pid == 0) {
        execl("./piekarz", "piekarz", NULL);
        perror("Bład uruchamiania piekarza");
        exit(1);
    }
    */
    /* ===== KASJERZY ===== 
    for (int i = 0; i < LICZBA_KAS; i++) {
        kasjer_pid[i] = fork();
        if (kasjer_pid[i] == 0) {
            char id[8];
            sprintf(id, "%d", i);
            execl("./kasjer", "kasjer", id, NULL);
            perror("Bład uruchamiania kasjerów");
            exit(1);
        }
    }
    */

    /* ===== SYMULACJA CZASU ===== */
    int czas_symulacji = CZAS_TRWANIA * 6; // w 10 minutach
    for (int t = 0; t < czas_symulacji; t++) {
        sleep(0.1); // 1 sekunda rzeczywista = 1 minuta symulacyjna

        //sem_wait(sem);
        //shm->aktualny_czas = START_TIME + t;
        //sem_post(sem);

        tick();

    }




    sleep(5);
    //wyslij_inwentaryzacje();

    sleep(5);
    //wyslij_ewakuacje();

    sem_wait(sem);
    shm->sklep_otwarty = 0;
    sem_post(sem);

    /* ===== CZEKAJ NA DZIECI ===== */
    for (int i = 0; i < 1 + LICZBA_KAS + LICZBA_KLIENTOW; i++)
        wait(NULL);

    /* ===== RAPORT ===== */
    printf("\n[KIEROWNIK] RAPORT KONCOWY\n");

    for (int p = 0; p < MAX_PRODUKTOW; p++) {
        int sprzedano = 0;
        for (int k = 0; k < MAX_KAS; k++)
            sprzedano += shm->sprzedane[k][p];

        printf("Produkt %d: wyprodukowano=%d sprzedano=%d\n",
               p, shm->wyprodukowane[p], sprzedano);
    }

    ipc_cleanup(1);
    printf("[KIEROWNIK] Koniec\n");
    return 0;
}


