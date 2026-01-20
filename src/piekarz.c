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
    int cook_cd = 0;
    while (!ewakuacja && shm->sklep_otwarty) {
        //loguj("PIEKARZ", "starttickwait");
        
        //sem_tick_start_wait();
        if (!(!ewakuacja && shm->sklep_otwarty))
        {
            break;
        }
       // loguj("PIEKARZ", "starttickcritical");
        //loguj("PIEKARZ", "semtickstartwait");
        int sztuk = 1 + rand() % 3; // losowa liczba sztuk
        int produkt_index = rand() % 10; // losowy produkt
        produkt_t produkt = produkty[produkt_index];
        sem_wait_mem(); // ochrona pamięci
        if (cook_cd > 0) {
            cook_cd--;
            sem_post_mem();
            //sem_tick_done_post();
            continue; // czekaj na kolejny tick
        }
        if (shm->podajniki[produkt.id].count>=64) {
            sem_post_mem();
            char buf[64];
            sprintf(buf, "Podajnik pełen: id %d\n", produkt.id);
            loguj("PIEKARZ", buf);
            //sem_tick_done_post();
            cook_cd = CZAS_GOTOWANIA; // ustaw czas gotowania
            continue; // podajnik pełny, spróbuj później
        }
        for (int i = 0; i < sztuk; i++) {
            if (dodaj_produkt(produkt) == 0) {
                shm->wyprodukowane[produkt.id]++;
                char buf[64];
                sprintf(buf, "Wyprodukowano produkt %s\n", produkt.name);
                loguj("PIEKARZ", buf);
            }
        }
        cook_cd = CZAS_GOTOWANIA; // ustaw czas gotowania
        sem_post_mem();
        //loguj("PIEKARZ", "sempostmem");
        //sem_tick_done_post();
        //loguj("PIEKARZ", "semtickdonepost");
        //tick_end(shm->aktualna_liczba_procesow);
        //sleep(0.1); 
    }

    loguj("PIEKARZ", "Koniec pracy - ewakuacja");

    ipc_cleanup(0);
    return 0;
}
