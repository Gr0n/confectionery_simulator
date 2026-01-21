#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "ipc.h"

#define LOG_FILE "log"
#define RAPORT_FILE "raport"
void loguj(const char *proces, const char *tekst) {
    FILE *f;
    time_t t;
    struct tm *tm_info;
    /* SEKCJA KRYTYCZNA */
    sem_wait_logger();

    f = fopen(LOG_FILE, "a");
    if (!f) {
        sem_post_logger();
        return;
    }

    time(&t);
    tm_info = localtime(&t);

    fprintf(f,
        "[%02d:%02d:%02d] PID=%d %-10s | %s\n",
        tm_info->tm_hour,
        tm_info->tm_min,
        tm_info->tm_sec,
        getpid(),
        proces,
        tekst
    );

    fflush(f);
    fclose(f);

    sem_post_logger();
}

void raport(const char *proces, const char *tekst) {
    FILE *f;
    time_t t;
    struct tm *tm_info;
    /* SEKCJA KRYTYCZNA */
    sem_wait_logger();

    f = fopen(RAPORT_FILE, "a");
    if (!f) {
        sem_post_logger();
        return;
    }

    fprintf(f,
        "[%-10s] | %s\n",
        proces,
        tekst
    );

    fflush(f);
    fclose(f);

    sem_post_logger();
}