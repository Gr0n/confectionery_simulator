#ifndef IPC_H
#define IPC_H
#define _XOPEN_SOURCE 700
#include <semaphore.h>
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // shm_open, mmap
#include <unistd.h>     // ftruncate
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "podajnik.h"

/* ================= STAŁE ================= */
#define D_PRODUKTOW 10
#define SHM_NAME "/shm_sklep"
#define SEM_MEM_NAME "/sem_mem"
#define SEM_KLIENT_NAME "/sem_klient"
#define SEM_LOGGER_NAME "/sem_logger"

#define FIFO_KASA1 "/tmp/kasa1_fifo"
#define FIFO_KASA2 "/tmp/kasa2_fifo"
/* ================= STRUKTURA PAMIĘCI WSPÓŁDZIELONEJ ================= */
typedef struct {
    podajnik_t podajniki[D_PRODUKTOW];
    int piekarnia_otwarta;    // 1 = otwarta, 0 = zamknięta
    int sklep_otwarty;             // 1 = otwarty, 0 = zamknięty
    int wyprodukowane[D_PRODUKTOW];
    int sprzedane[2][D_PRODUKTOW]; // sprzedane na kasach
    int aktualny_czas;
    int inwentaryzacja;
    int ewakuacja;
    int kasy_otwarte[2];
} shm_data_t;

#define FIFO_NAME_LEN 64

typedef struct {
    int klient_id;
    int produkt_id[10];
    char reply_fifo[FIFO_NAME_LEN];
} fifo_req_t;




extern shm_data_t *shm;   
extern sem_t *sem_mem;
extern sem_t *sem_klient;
extern sem_t *sem_logger;
/* ========================= FUNKCJE IPC ========================= */

/* ===== Pamięć współdzielona ===== */
int ipc_init(int create);           // tworzy lub otwiera SHM i semafory
shm_data_t* ipc_get_shm(void);      // zwraca wskaźnik do pamięci
void ipc_cleanup(int remove_all);    // odłącza i ewentualnie usuwa SHM/semafory

/* ===== Semafor pamięci ===== */
void sem_wait_mem(void);
void sem_post_mem(void);

/* ===== Semafor loggera ===== */
int sem_logger_init(int create);
void sem_wait_logger(void);
void sem_post_logger(void);
void sem_logger_cleanup(int remove_all);

/* ===== Semafor limitu klientów ===== */
int sem_klientlimit_init(int create, int limit);
int sem_klientlimit_wait(void);
int sem_klientlimit_post(void);
void sem_klientlimit_cleanup(int remove_all);

#endif /* IPC_H */
