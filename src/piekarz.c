#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include "ipc.h"
#include "logger.h"
#include "podajnik.h"

/* Globalna tablica podajników */
podajnik_t podajniki[D_PRODUKTOW];


static volatile sig_atomic_t inwentaryzacja = 0;
static volatile sig_atomic_t ewakuacja = 0;

sem_t *sem;

void sig_inwentaryzacja(int sig) {
    (void)sig;
    inwentaryzacja = 1;
    loguj("PIEKARZ", "Otrzymano sygnal inwentaryzacji");
}

void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    loguj("PIEKARZ", "Otrzymano sygnal ewakuacji");
}



/* Dodaj produkt na podajnik FIFO */
int dodaj_produkt(produkt_t produkt) {
    return podajnik_push_shm(&podajniki[produkt.id], &produkt);  // 1 sztuka
}

/* Funkcja główna piekarza */
int main() {
    srand(time(NULL) ^ getpid());

    signal(SIGUSR1, sig_inwentaryzacja);
    signal(SIGUSR2, sig_ewakuacja);

    if (ipc_init(0) == -1) {
        perror("ipc_init piekarz");
        exit(1);
    }

    shm = ipc_get_shm();
    for(int p = 0; p < D_PRODUKTOW; p++){
        podajnik_init_shm(&shm->podajniki[p]);
    }

    // Inicjalizacja FIFO
    for (int i = 0; i < D_PRODUKTOW; i++) {
        podajniki[i].head = 0;
        podajniki[i].tail = 0;
        podajniki[i].count = 0;
    }

    loguj("PIEKARZ", "Start pracy piekarza");

    while (!ewakuacja) {
        produkt_t test_produkt1 = {0, "Rogalik", 5};
        int sztuk = 1 + rand() % 3; // losowa liczba sztuk

        sem_wait(sem); // ochrona pamięci

        for (int i = 0; i < sztuk; i++) {
            if (dodaj_produkt(test_produkt1) == 0) {
                shm->wyprodukowane[test_produkt1.id]++;
                char buf[64];
                sprintf(buf, "Wyprodukowano produkt %s\n", test_produkt1.name);
                loguj("PIEKARZ", buf);
            }
        }

        sem_post(sem);

        sleep(1); // czas pieczenia
    }

    loguj("PIEKARZ", "Koniec pracy - ewakuacja");

    ipc_cleanup(0);
    return 0;
}
