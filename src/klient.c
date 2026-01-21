#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipc.h"
#include "logger.h"
#include "podajnik.h"

#define MAX_ZAKUPOW 5
#define NAME "KLIENT"
static volatile sig_atomic_t ewakuacja = 0;
char reply_fifo[64];

//obsługa sygnału ewakuacji
void sig_ewakuacja(int sig) {
    (void)sig;
    ewakuacja = 1;
    loguj("KLIENT", "Otrzymano sygnal EWAKUACJA");
}


//Losowanie listy zakupów klienta
void losuj_zakupy(int zakupy[D_PRODUKTOW]) {
    for (int i = 0; i < D_PRODUKTOW; i++)
        zakupy[i] = 0;
    int rozne_produkty = (rand() % 14) + 2;
    for (int i = 0; i < rozne_produkty; i++)
        zakupy[rand()%D_PRODUKTOW] += 1;
}

//czyszczenie fifo w razie awarii
void cleanup() {
    unlink(reply_fifo);
}

void sigint_handler(int sig) {
    (void)sig;
    unlink(reply_fifo);
    exit(0);
}

int main() {
    //obsługa czyszczenia fifo w razie zamkniecia
    atexit(cleanup);
    signal(SIGINT, sigint_handler);
    srand(getpid() ^ time(NULL));

    sprintf(reply_fifo, "/tmp/klient_%d_fifo", getpid());

    //otwarcie fifo dla paragonu
    if (mkfifo(reply_fifo, 0600))
    {
        perror("mkfifo reply_fifo");
        exit(1);
    }

    //Połączenie sygnału ewakuacji
    signal(SIGUSR2, sig_ewakuacja);

    if (ipc_init(0) == -1) {
        perror("ipc_init klient");
        exit(1);
    }

    shm = ipc_get_shm();

    if (sem_klientlimit_init(0, 0) == -1) {
        perror("sem_limit_init klient");
        exit(1);
    }

    /* ===== WEJSCIE DO SKLEPU ===== */
    //oczekiwanie na opuszczenie semafora do wejscia do sklepu
    loguj(NAME, "Czeka na wejscie do sklepu");
    sem_klientlimit_wait();
    //czyszczenie koszyka
    int koszyk[10] = {0};
    if (!ewakuacja && shm->sklep_otwarty==1)
    {
    loguj(NAME, "Wszedl do sklepu");
    

    /* ===== ZAKUPY ===== */

    //tworzenie listy zakupow
    int zakupy[10] = {0};
    losuj_zakupy(zakupy);
    char buf_zakupy[512];
    int pos = 0;
    
    pos += snprintf(buf_zakupy + pos, sizeof(buf_zakupy) - pos,
                    "Lista zakupow: ");

    for (int i = 0; i < D_PRODUKTOW; i++) {
        if (ewakuacja || shm->sklep_otwarty == 0) break;
        if (zakupy[i] > 0) {
            pos += snprintf(buf_zakupy + pos, sizeof(buf_zakupy) - pos,
                            "%d x %d, ", zakupy[i], i);
            if (pos >= (int)sizeof(buf_zakupy) - 1) break;
        }
    }

    loguj(NAME, buf_zakupy);
    //Pobieranie produktów z podajników
    for (int i = 0; i < D_PRODUKTOW; i++) {
        if (ewakuacja || shm->sklep_otwarty==0) break;

        //Klient idzie po koleji do podajników jeśli ma produkt na swojej liscie
        int p = zakupy[i];
        sem_wait_mem();
        for (int j = 0; j < p; j++) {
            if (ewakuacja) break;
            char buf[64];
            sprintf(buf, "Szukam produktu %d na podajnikach", i);
            loguj(NAME, buf);
            produkt_t produkt;
            if (podajnik_pop_shm(&shm->podajniki[i], &produkt) == -1) {
                loguj(NAME, "Brak produktu na podajniku, idzie do następnego");
                break;
            }
            koszyk[i]++;
            sprintf(buf, "Wzial produkt %d", i);
            loguj(NAME, buf);
            buf[0] = '\0';
        }
        //odblokowanie semafora pamięci wsp.
        sem_post_mem();        
    }


    //obsluga logu w czasie wychodzenia ze sklepu
    if (ewakuacja) {
        loguj(NAME, "Przerwal zakupy i opuszcza sklep");
    } else {
        loguj(NAME, "Zakonczyl zakupy i idzie do kasy");
    }


    }
    //Jeśli sklep jest zamknięty klient wychodzi ze sklepu
    if (ewakuacja || shm->sklep_otwarty==0) {
        
        if (ewakuacja)
        {
            for (int i=0; i<10; i++)
            {
                char bufor_zak[64];
                sprintf(bufor_zak, "Odkłada produktów %d x %d do kosza", i, koszyk[i]);
                loguj(NAME, bufor_zak);
            }
        }

        unlink(reply_fifo);
        sem_klientlimit_post();
        loguj(NAME, "Opuscil sklep");
        ipc_cleanup(0);
        return 0;
    }
    //Inaczej klient idzie do kasy
    int b1, b2;
    //otwarcie fifo kas
    int fd_kasa1 = open(FIFO_KASA1, O_WRONLY | O_NONBLOCK);
    int fd_kasa2 = open(FIFO_KASA2, O_WRONLY | O_NONBLOCK);
    if ((fd_kasa1 == -1 && fd_kasa2 == -1) || shm->sklep_otwarty==0) {
        loguj(NAME, "Sklep zamknięty, pomijam kasy");
        
    }
    else
    {
        //sprawdzenie wielkości kolejek w kasach
        ioctl(fd_kasa1, FIONREAD, &b1);
        ioctl(fd_kasa2, FIONREAD, &b2);

        //utworzenie komunikatu dla fifo
        fifo_req_t msg = {0};
        msg.klient_id = getpid();
        memcpy(msg.produkt_id, koszyk, sizeof(msg.produkt_id));
        strcpy(msg.reply_fifo, reply_fifo);

        //Porównanie wielkości kas, klient idzie to kasy z mniejsza kolejka lub do kasy 1 jesli kasa 2 jest zamknieta
        if (b1 > b2 && shm->kasy_otwarte[1]==1) {
            close(fd_kasa1);
            loguj(NAME, "Idzie do kasy 2");
            if (write(fd_kasa2, &msg, sizeof(msg)) == -1) {
                perror("write to kasa2");
            }
            close(fd_kasa2);
        }
        else {
            close(fd_kasa2);
            loguj(NAME, "Idzie do kasy 1");
            if (write(fd_kasa1, &msg, sizeof(msg)) == -1) {
                perror("write to kasa1");
            }
            close(fd_kasa1);
        }

        //oczekiwanie na komunikat powrotny (paragon)
        loguj(NAME, "Czeka na paragon");
        int fd_reply = open(reply_fifo, O_RDONLY);
        if (fd_reply == -1) {
            perror("open reply_fifo");
        }
        loguj(NAME, "Otrzymal paragon");
        char paragon[1024];
        paragon[0] = '\0';
        if (read(fd_reply, paragon, sizeof(paragon)) == -1) {
            perror("read from reply_fifo");
        }
        char buf[1024];
        sprintf(buf, "PARAGON: %s\n", paragon);
        loguj(NAME, buf);
        
        close(fd_reply);
        unlink(reply_fifo);
        
    }
    /* ===== WYJSCIE ===== */
    //opuszczenie sklepu
    sem_klientlimit_post();
    loguj(NAME, "Opuscil sklep");

    ipc_cleanup(0);
    sem_klientlimit_cleanup(0);
    return 0;
}
