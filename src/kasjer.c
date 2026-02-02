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

//sygnały
static volatile sig_atomic_t inwentaryzacja = 0;
static volatile sig_atomic_t ewakuacja = 0;

//wskaznik do pamięci współdzielonej
sem_t *sem;

//lista produktów
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
int id;
int sprzedane_produkty[10] = {0,0,0,0,0,0,0,0,0,0};
int kasa_otwarta = 0;
char paragon[1024];

void cleanup() {
    if (id==1){unlink(FIFO_KASA1);}
    if (id==2){unlink(FIFO_KASA2);}
}
//obsługa sygnału inwentaryzacji
void sig_inwentaryzacja(int sig) {
    (void)sig;
    inwentaryzacja = 1;
    loguj(NAME, "Otrzymano sygnal inwentaryzacji");
}
//obsługa sygnału ewakuacji
void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    loguj(NAME, "Otrzymano sygnal ewakuacji");
}
void sigint_handler(int sig) {
    (void)sig;
    cleanup();
    exit(0);
}
//inicjalizacja fifo kas
void init_fifo(int id) {
    if (id == 1) {
        if (mkfifo(FIFO_KASA1, 0600) == -1) {
            perror("mkfifo kasa1");
        }
    } else {
        if (mkfifo(FIFO_KASA2, 0600) == -1) {
            perror("mkfifo kasa2");
        }
    }

}

//funkcja sprzedająca produkt i dodająca go do paragonu
void sprzedaj_produkt(produkt_t produkt, int ilosc) {
    sprzedane_produkty[produkt.id]+=ilosc;
    char buf[64];
    sprintf(buf, "Sprzedano produkt %s*%d\n", produkt.name, ilosc);
    loguj(NAME, buf);
    buf[0] = '\0';
    sprintf(buf, "id: %d, %d szt. %.2f zł\n", produkt.id, ilosc, produkt.cena*ilosc);
    strcat(paragon, buf);
    loguj(NAME, buf);
}

//funkcja czyszcząca paragon
void wyczysc_paragon() {
    paragon[0] = '\0';
}

//funkcja podsumowująca inwentaryzację
void podsumowanie(int id){
    sem_wait_logger();
    char buf_title[64];
    sprintf(buf_title, "Kasjer %d - inwentaryzacja", id);
    raport(NAME, buf_title);
    for(int i=0;i<10;i++){
        char buf[64];
        sprintf(buf, "Produkt %s, sprzedano: %d szt.\n", produkty[i].name, sprzedane_produkty[i]);
        raport(NAME, buf);
    }
    sem_post_logger();
}

// FUNKCJA MAIN KASJERA
int main(int argc, char **argv) {
    atexit(cleanup);
    signal(SIGINT, sigint_handler);
    //sprawdzenie argumentów
    if (argc < 2) {
        fprintf(stderr, "Brak argumentu ID\n");
        return 1;
    }
    //pobranie ID kasjera
    id = atoi(argv[1]);
    //inicjalizacja fifo kas na podstawie ID
    init_fifo(id);
    int fd_kasa;
    if (id == 1) {
        fd_kasa = open(FIFO_KASA1, O_RDONLY | O_NONBLOCK);
    } else {
        fd_kasa = open(FIFO_KASA2, O_RDONLY | O_NONBLOCK);
    }
    if (fd_kasa == -1) {
        perror("open fifo kasa");
        exit(1);
    }

    //inicjalizacja generatora liczb losowych
    srand(time(NULL) ^ getpid());

    //ustawienie obsługi sygnałów
    signal(SIGUSR1, sig_inwentaryzacja);
    signal(SIGUSR2, sig_ewakuacja);
    
    //inicjalizacja IPC
    if (ipc_init(0) == -1) {
        perror("ipc_init kasjer");
        exit(1);
    }
    shm = ipc_get_shm();

    //pobranie wielkości wiadomości w fifo kasy
    int b;
    if (ioctl(fd_kasa, FIONREAD, &b)) {
        perror("ioctl FIONREAD");
        exit(1);
    }
    loguj(NAME, "Kasjer gotowy do pracy");
    paragon[0] = '\0';

    //Główna pętla kasjera
    while ((!ewakuacja && shm->piekarnia_otwarta==1) || b > 0) {
        fifo_req_t msg;
        kasa_otwarta = shm->kasy_otwarte[id - 1];

        //pobranie wielkości wiadomości w fifo kasy
        if (ioctl(fd_kasa, FIONREAD, &b))
        {
            perror("ioctl FIONREAD");
            break;
        }
        if (ewakuacja && b == 0) 
        {
            break;
        }
        if (!shm->piekarnia_otwarta && b == 0) 
        {
            break;
        }
        //sprawdzenie czy kasa jest pusta/otwarta
        if (b == 0 && !kasa_otwarta) {
            continue;
        }
        if (b == 0 && kasa_otwarta) {
            continue;
        }
        ssize_t r = read(fd_kasa, &msg, sizeof(msg));
        if (r <= 0) {
            if (r == 0) {
                // FIFO zamknięte po drugiej stronie
                continue;
            }
            if (errno == EAGAIN) {
                continue;
            }
            perror("read");
            continue;
        }
        loguj(NAME, "Obsługa klienta...");

        //otwarcie fifo klienta do pisania
        int fd_reply = open(msg.reply_fifo, O_WRONLY);
        if (fd_reply == -1) {
            perror("open fifo reply kasa");
            continue;
        }

        //generowanie paragonu z listy zakupow klienta
        for (int i = 0; i < 10; i++) {
            char buf[64];
            sprintf(buf, "Produkt ID: %d, Ilość: %d\n", i, msg.produkt_id[i]);
            loguj(NAME, buf);
            if (msg.produkt_id[i] == 0)
                continue;
            sprzedaj_produkt(produkty[i], msg.produkt_id[i]);
        }
        loguj(NAME, "Wystawianie paragonu:");
        if (write(fd_reply, paragon, strlen(paragon) + 1) == -1) {
            perror("write paragon");
        }
        close(fd_reply);
        wyczysc_paragon();
    }
    //obsługa inwentaryzacji
    if (inwentaryzacja) {
        podsumowanie(id);
    }
    char buf[64];
    sprintf(buf, "Koniec pracy kasjera %d", id);
    loguj(NAME, buf);

    //zamkniecie fifo kas
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
