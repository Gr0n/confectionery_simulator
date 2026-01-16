#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // mmap, shm_open
#include <semaphore.h>
#include <errno.h>

/* ========================= GLOBALNE ========================= */
shm_data_t *shm = NULL;
int shm_fd = -1;

sem_t *sem_mem = NULL;
sem_t *sem_klient = NULL;

/* ========================= Semafory ========================= */

int sem_klientlimit_init(int create, int limit) {
    if (create) {
        sem_klient = sem_open(SEM_KLIENT_NAME, O_CREAT | O_EXCL, 0666, limit);
        if (sem_klient == SEM_FAILED) {
            if (errno == EEXIST) {
                sem_klient = sem_open(SEM_KLIENT_NAME, 0);
            } else {
                perror("sem_open create klient");
                return -1;
            }
        }
    } else {
        sem_klient = sem_open(SEM_KLIENT_NAME, 0);
        if (sem_klient == SEM_FAILED) { perror("sem_open get klient"); return -1; }
    }
    return 0;
}

int sem_klientlimit_wait(void) {
    if (!sem_klient) return -1;
    return sem_wait(sem_klient);
}

int sem_klientlimit_post(void) {
    if (!sem_klient) return -1;
    return sem_post(sem_klient);
}

void sem_klientlimit_cleanup(int remove_all) {
    if (sem_klient) {
        sem_close(sem_klient);
        if (remove_all) sem_unlink(SEM_KLIENT_NAME);
        sem_klient = NULL;
    }
}

/* ========================= Pamięć współdzielona ========================= */

int ipc_init(int create) {
    /* Tworzenie lub otwarcie SHM */
    if (create) {
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) { perror("shm_open create"); return -1; }

        if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) {
            perror("ftruncate"); return -1;
        }
    } else {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) { perror("shm_open open"); return -1; }
    }

    shm = mmap(NULL, sizeof(shm_data_t),
               PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return -1; }

    /* Zerowanie danych tylko przy tworzeniu */
    if (create) {
        memset(shm, 0, sizeof(shm_data_t));
        shm->sklep_otwarty = 1;
    }

    /* Semafor pamięci */
    if (create) {
        sem_mem = sem_open(SEM_MEM_NAME, O_CREAT | O_EXCL, 0666, 1);
        if (sem_mem == SEM_FAILED) {
            if (errno == EEXIST) {
                sem_mem = sem_open(SEM_MEM_NAME, 0);
            } else {
                perror("sem_open create mem"); return -1;
            }
        }
    } else {
        sem_mem = sem_open(SEM_MEM_NAME, 0);
        if (sem_mem == SEM_FAILED) { perror("sem_open get mem"); return -1; }
    }

    return 0;
}

shm_data_t* ipc_get_shm(void) {
    return shm;
}

/* Operacje na semaforze pamięci */
void sem_wait_mem(void) {
    if (sem_mem) sem_wait(sem_mem);
}

void sem_post_mem(void) {
    if (sem_mem) sem_post(sem_mem);
}

void ipc_cleanup(int remove_all) {
    if (shm) {
        munmap(shm, sizeof(shm_data_t));
        shm = NULL;
    }

    if (shm_fd != -1) {
        close(shm_fd);
        if (remove_all) shm_unlink(SHM_NAME);
        shm_fd = -1;
    }

    if (sem_mem) {
        sem_close(sem_mem);
        if (remove_all) sem_unlink(SEM_MEM_NAME);
        sem_mem = NULL;
    }

    sem_klientlimit_cleanup(remove_all);
}
