#include "queue.h"

#include <stdlib.h>
#include <string.h>

queue *queueInit (void)
{
    queue *q;

    q = (queue *)malloc(sizeof (queue));
    if (q == NULL) return NULL;

    for (int i = 0; i < QUEUESIZE; i++) {
        q->buf[i] = NULL;
    }

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);

    return (q);
}

void queueDelete (queue *q)
{
    if (q == NULL) {
        return;
    }

    while (!q->empty) {
        free(q->buf[q->head]);
        q->buf[q->head] = NULL;
        q->head++;

        if (q->head == QUEUESIZE) {
            q->head = 0;
        }

        if (q->head == q->tail) {
            q->empty = 1;
            break;
        }
    }

    pthread_mutex_destroy (q->mut);
    free (q->mut);
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}

void queueAdd (queue *q, const char *in, size_t len)
{
    char *copy;

    // Copy libwebsockets buffer (pointer) "in" in queue buf, in is then discarded or reused by libwebsockets. 
    // Queue owns this memory until queueDel frees it.
    if (q == NULL || in == NULL) {
        return;
    }

    copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return;
    }

    memcpy(copy, in, len);
    copy[len] = '\0';

    q->buf[q->tail] = copy;
    q->tail++;

    if (q->tail == QUEUESIZE){
        q->tail = 0;
    }

    if (q->tail == q->head){
        q->full = 1;
    }

    q->empty = 0;

    return;
}

void queueDel (queue *q, char **out)
{
    // Queue-owned payload: caller must free(*out) after use.
    if (q == NULL || out == NULL || q->buf[q->head] == NULL) {
        *out = NULL;
        return;
    }

    *out = q->buf[q->head];
    q->buf[q->head] = NULL;

    q->head++;

    if (q->head == QUEUESIZE){
        q->head = 0;
    }

    if (q->head == q->tail){
        q->empty = 1;
    }

    q->full = 0;

    return;
}