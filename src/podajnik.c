#include "podajnik.h"

#include <stdio.h>     
#include <stdlib.h>     
#include <string.h>     
#include <pthread.h>
void podajnik_init(podajnik_t *p) {
    p->head = 0;
    p->tail = 0;
    p->count = 0;
}

/* Push - dodaj ilość sztuk na koniec FIFO podajnika */
int podajnik_push(podajnik_t *p, produkt_t produkt) {
    if (p->count == MAX_PODAJNIK)
        return -1;

    p->produkty[p->tail] = produkt;
    p->tail = (p->tail + 1) % MAX_PODAJNIK;
    p->count++;

    return 0;
}

int podajnik_push_shm(podajnik_t *p, const produkt_t *in) {
    int ret;

    if (p == NULL || in == NULL)
        return -1;

    pthread_mutex_lock(&p->mutex);
    ret = podajnik_push(p, *in);
    pthread_mutex_unlock(&p->mutex);

    return ret;
}

/* Pop - pobierz ilość sztuk z przodu FIFO podajnika */
int podajnik_pop(podajnik_t *p, produkt_t *out) {
    if (p->count == 0)
        return -1;

    *out = p->produkty[p->head];
    p->head = (p->head + 1) % MAX_PODAJNIK;
    p->count--;

    return 0;
}

int podajnik_pop_shm(podajnik_t *p, produkt_t *out) {
    int ret;

    pthread_mutex_lock(&p->mutex);
    ret = podajnik_pop(p, out);
    pthread_mutex_unlock(&p->mutex);

    return ret;
}


int podajnik_is_empty(const podajnik_t *p) {
    return p->count == 0;
}

int podajnik_is_full(const podajnik_t *p) {
    return p->count == MAX_PODAJNIK;
}

/* Pobierz ilość bez usuwania */
int podajnik_peek(const podajnik_t *p, produkt_t *out) {
    if (p->count == 0)
        return -1;

    *out = p->produkty[p->head];
    return 0;
}

