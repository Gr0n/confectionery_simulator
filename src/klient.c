#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "ipc.h"
#include "logger.h"

#define MAX_ZAKUPOW 3

static volatile sig_atomic_t ewakuacja = 0;

shm_data_t *shm;
sem_t *sem_ipc;
sem_t *sem_limit;

/* ================= SYGNALY ================= */

void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    loguj("KLIENT", "Otrzymano sygnal EWAKUACJA");
}

/* ================= ZAKUPY ================= */

void losuj_zakupy(int zakupy[MAX_ZAKUPOW]) {
    for (int i = 0; i < MAX_ZAKUPOW; i++)
        zakupy[i] = rand() % MAX_PRODUKTOW;
}

/* ================= MAIN ================= */

int main() {
    srand(getpid() ^ time(NULL));

    signal(SIGUSR2, sig_ewakuacja);

    if (ipc_init(0) == -1) {
        perror("ipc_init klient");
        exit(1);
    }

    sem_ipc = ipc_get_sem();
    shm = ipc_get_shm();

    if (sem_klientlimit_init(0) == -1) {
        perror("sem_limit_init klient");
        exit(1);
    }
    sem_limit = sem_klientlimit_get();

    /* ===== WEJSCIE DO SKLEPU ===== */
    loguj("KLIENT", "Czeka na wejscie do sklepu");
    sem_wait(sem_limit);
    loguj("KLIENT", "Wszedl do sklepu");

    /* ===== ZAKUPY ===== */
    int zakupy[MAX_ZAKUPOW];
    losuj_zakupy(zakupy);

    for (int i = 0; i < MAX_ZAKUPOW; i++) {
        if (ewakuacja) break;

        int p = zakupy[i];

        sem_wait(sem_ipc);
        shm->sprzedane[0][p]++;  /* uproszczenie: klient zawsze idzie do kasy 0 */
        sem_post(sem_ipc);

        char buf[64];
        sprintf(buf, "Wzial produkt %d", p);
        loguj("KLIENT", buf);

        sleep(1);
    }

    if (ewakuacja) {
        loguj("KLIENT", "Przerwal zakupy i opuszcza sklep");
    } else {
        loguj("KLIENT", "Zakonczyl zakupy i idzie do kasy");
    }

    /* ===== WYJSCIE ===== */
    sem_post(sem_limit);
    loguj("KLIENT", "Opuscil sklep");

    ipc_cleanup(0);
    sem_klientlimit_cleanup(0);
    return 0;
}
