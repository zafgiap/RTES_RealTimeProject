#ifndef THREADS_H
#define THREADS_H

#include <signal.h>
#include "queue.h"
#include "jsonParse.h"

extern volatile sig_atomic_t running;
extern pthread_mutex_t monitor_mut;
extern pthread_cond_t monitor_tick;
extern int monitor_tick_pending;

void *producer(void *arg);
void *consumer(void *arg);
void *monitor(void *arg);

void JSONparse(char *data);

#endif