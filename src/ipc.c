#define _XOPEN_SOURCE 700
#include "ipc.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static shm_data_t *shm = NULL;
static sem_t *sem = NULL;

int ipc_init(int create) {
    int fd;

    if (create) {
        fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (fd == -1) {
            perror("shm_open create");
            return -1;
        }

        if (ftruncate(fd, sizeof(shm_data_t)) == -1) {
            perror("ftruncate");
            return -1;
        }
    } else {
        fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd == -1) {
            perror("shm_open open");
            return -1;
        }
    }

    shm = mmap(NULL, sizeof(shm_data_t),
               PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0);

    if (shm == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    if (create) {
        memset(shm, 0, sizeof(shm_data_t));
        shm->sklep_otwarty = 1;
    }

    if (create) {
        sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    } else {
        sem = sem_open(SEM_NAME, 0);
    }

    if (sem == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }

    return 0;
}

shm_data_t* ipc_get_shm() {
    return shm;
}

sem_t* ipc_get_sem() {
    return sem;
}

void ipc_cleanup(int unlink_all) {
    if (shm) {
        munmap(shm, sizeof(shm_data_t));
        shm = NULL;
    }

    if (sem) {
        sem_close(sem);
        sem = NULL;
    }

    if (unlink_all) {
        shm_unlink(SHM_NAME);
        sem_unlink(SEM_NAME);
    }
}
