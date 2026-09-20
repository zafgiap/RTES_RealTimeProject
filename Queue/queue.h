#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

#define QUEUESIZE 50

typedef struct {
    char *buf[QUEUESIZE]; // Stores the JSON formatted data as null-terminated strings
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

extern queue *producer_fifo; // Shared pointer to queue struct across the whole program

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, const char *in, size_t len);
void queueDel (queue *q, char **out);

#endif