#ifndef PODAJNIK_H
#define PODAJNIK_H

#include <stdio.h>     
#include <stdlib.h>     
#include <string.h>     
#include <pthread.h>
#define MAX_PODAJNIK 64
typedef struct {
    int id;                     // unikalne ID produktu
    char name[32];              // nazwa produktu
    double cena;                // cena produktu
} produkt_t;

/* ===== FIFO na podajnikach ===== */
typedef struct {
    produkt_t produkty[MAX_PODAJNIK];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
} podajnik_t;
//pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);



void podajnik_init(podajnik_t *p);

/* Push - dodaj ilość sztuk na koniec FIFO podajnika */
int podajnik_push(podajnik_t *p, produkt_t produkt);

/* Pop - pobierz ilość sztuk z przodu FIFO podajnika; zwraca -1 jeśli pusty */
int podajnik_pop(podajnik_t *p, produkt_t *out) ;
int podajnik_pop_shm(podajnik_t *p, produkt_t *out);
/* Sprawdzenia stanu */
int podajnik_is_empty(const podajnik_t *p);
int podajnik_is_full(const podajnik_t *p);

/* Pobierz ilość bez usuwania */
int podajnik_peek(const podajnik_t *p, produkt_t *out);

#endif // PODAJNIK_H