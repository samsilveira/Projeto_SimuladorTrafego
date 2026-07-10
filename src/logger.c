#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include "logger.h"

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char* filename) {
    log_file = fopen(filename, "w"); // Sobrescreve a cada nova simulação
}

void log_event(const char* format, ...) {
    if (!log_file) return;

    pthread_mutex_lock(&log_mutex);
    
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    fprintf(log_file, "[%02d:%02d:%02d] ", local->tm_hour, local->tm_min, local->tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file); // Força a gravação no disco imediatamente
    
    pthread_mutex_unlock(&log_mutex);
}

void log_close() {
    if (log_file) fclose(log_file);
}