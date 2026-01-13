#pragma once
#include <semaphore.h>

#define SHM_NAME "/ciastkarnia_shm"
#define SEM_NAME "/ciastkarnia_sem"
#define SEM_LIMIT_NAME "/ciastkarnia_sem_limit"

#define MAX_PRODUKTOW 12
#define MAX_KAS 2

typedef struct {
    int sklep_otwarty;
    int ewakuacja;
    int inwentaryzacja;

    int wyprodukowane[MAX_PRODUKTOW];
    int sprzedane[MAX_KAS][MAX_PRODUKTOW];
} shm_data_t;

/* inicjalizacja / podłączenie */
int ipc_init(int create);

/* dostęp do zasobów */
shm_data_t* ipc_get_shm();
sem_t* ipc_get_sem();

/* sprzątanie */
void ipc_cleanup(int unlink_all);

/* semafor ograniczający liczbę klientów w sklepie */
int sem_klientlimit_init(int create, int limit);
sem_t* sem_klientlimit_get();
void sem_klientlimit_cleanup(int unlink_all);