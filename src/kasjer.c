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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define NAME "KASJER"
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

int *sprzedane_produkty[10] = {0,0,0,0,0,0,0,0,0,0};
int kasa_otwarta = 0;
char paragon[128];

//shm_data_t *shm;
void sig_inwentaryzacja(int sig) {
    (void)sig;
    inwentaryzacja = 1;
    loguj(NAME, "Otrzymano sygnal inwentaryzacji");
}

void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    loguj(NAME, "Otrzymano sygnal ewakuacji");
}

void init_fifo() {
    if (mkfifo(FIFO_KASA1, 0666) == -1) {
        perror("mkfifo kasa1");
    }
    if (mkfifo(FIFO_KASA2, 0666) == -1) {
        perror("mkfifo kasa2");
    }
}


/* Dodaj produkt na podajnik FIFO */
void sprzedaj_produkt(produkt_t produkt) {
    sprzedane_produkty[produkt.id]++;
    char buf[64];
    sprintf(buf, "Sprzedano produkt %s\n", produkt.name);
    loguj(NAME, buf);
    sprintf(buf, "id: %d, %f zł\n", produkt.id, produkt.cena);
    strcat(paragon, buf);
}

void wyczysc_paragon() {
    paragon[0] = '\0';
}


/* Funkcja główna piekarza */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Brak argumentu ID\n");
        return 1;
    }
    int id = atoi(argv[1]);

    if (id==1){
        init_fifo();
    }

    int fd_kasa;
    if (id == 1) {
        fd_kasa = open(FIFO_KASA1, O_RDONLY | O_NONBLOCK);
    } else {
        fd_kasa = open(FIFO_KASA1, O_RDONLY | O_NONBLOCK);
    }

    if (fd_kasa == -1) {
        perror("open fifo kasa");
        exit(1);
    }

    srand(time(NULL) ^ getpid());

    signal(SIGUSR1, sig_inwentaryzacja);
    signal(SIGUSR2, sig_ewakuacja);
    
    if (ipc_init(0) == -1) {
        perror("ipc_init kasjer");
        exit(1);
    }

    shm = ipc_get_shm();

    loguj(NAME, "Kasjer gotowy do pracy");
    while (!ewakuacja && shm->sklep_otwarty) {
        fifo_req_t msg;
        kasa_otwarta = shm->kasy_otwarte[id - 1];
        //sem_wait_mem(); // ochrona pamięci
        int b;
        ioctl(fd_kasa, FIONREAD, &b);
        if(kasa_otwarta || b > 0 )
        {
            read(fd_kasa, &msg, sizeof(msg));

            int fd_reply = open(msg.reply_fifo, O_WRONLY);
            if (fd_reply == -1) {
                perror("open fifo reply kasa");
                continue;
            }
            for(int i=0; i<32; i++)
            {
                if(msg.produkt_id[i] == 0) // w wiadomości produkt_id powinien być zwiększony o 1
                    break;
                sprzedaj_produkt(produkty[msg.produkt_id[i]-1]);
            }
            write(fd_reply, paragon, sizeof(paragon));
            close(fd_reply);
            wyczysc_paragon();
        }
        //sem_post_mem();
    }

    loguj(NAME, "Koniec pracy - ewakuacja");
    if (id == 1) {
        close(fd_kasa);
        unlink(FIFO_KASA1);
    } else {
        close(fd_kasa);
        unlink(FIFO_KASA2);
    }
    ipc_cleanup(0);
    return 0;
}
