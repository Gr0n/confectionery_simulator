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
#define CZAS_GOTOWANIA 15 // czas gotowania w tickach
#define NAME "PIEKARZ"
static volatile sig_atomic_t inwentaryzacja = 0;
static volatile sig_atomic_t ewakuacja = 0;

sem_t *sem;

produkt_t produkty[10] = {{0, "Rogalik", 2},
{1, "Herbatnik", 1},
{2, "Babeczka", 5},
{3, "Karpatka", 8},
{4, "Wafel", 1},
{5, "Makaronik", 8},
{6, "Ptyś", 7},
{7, "Biszkopcik", 6},
{8, "Pierniczek", 3},
{9, "Sezamka", 4}};

int wyprodukowane_produkty[10] = {0};


//shm_data_t *shm;
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
    return podajnik_push_shm(&shm->podajniki[produkt.id], &produkt);  // 1 sztuka
}

//podsumowanie w razie inwentaryzacji
void podsumowanie(){
    sem_wait_logger();
    raport(NAME, "Inwentaryzacja - podsumowanie produkcji:\n");
    for(int i=0;i<10;i++){
        char buf[64];
        sprintf(buf, "Produkt %s, wyprodukowano: %d szt.\n", produkty[i].name, wyprodukowane_produkty[i]);
        raport(NAME, buf);
    }
    sem_post_logger();
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

    loguj("PIEKARZ", "Start pracy piekarza");
    //czas gotowania
    int cook_cd = 0;

    //Pętla główna piekarza
    while (!ewakuacja && shm->piekarnia_otwarta) {
        if (!(!ewakuacja && shm->piekarnia_otwarta))
        {
            break;
        }
        int sztuk = 1 + rand() % 3; // losowa liczba sztuk
        int produkt_index = rand() % 10; // losowy produkt
        produkt_t produkt = produkty[produkt_index];
        sem_wait_mem(); // ochrona pamięci
        //jeśli nie jest gotowy piec to kontynuuje
        if (cook_cd > 0) {
            cook_cd--;
            sem_post_mem();
            continue;
        }
        //Zpushowanie produktu do podajnika
        if (shm->podajniki[produkt.id].count>=64) {
            sem_post_mem();
            cook_cd = CZAS_GOTOWANIA;
            continue; // podajnik pełny
        }
        for (int i = 0; i < sztuk; i++) {
            if (dodaj_produkt(produkt) == 0) {
                shm->wyprodukowane[produkt.id]++;
                wyprodukowane_produkty[produkt.id]++;
            }
        }
        cook_cd = CZAS_GOTOWANIA; // ustaw czas gotowania
        sem_post_mem();
    }
    if (inwentaryzacja) {
        podsumowanie();
    }
    loguj("PIEKARZ", "Koniec pracy");

    ipc_cleanup(0);
    return 0;
}
