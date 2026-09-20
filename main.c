#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "queue.h"
#include "threads.h"
#include "wsConnectWrapper.h"

queue *producer_fifo = NULL;
volatile sig_atomic_t running = 1;

static void handle_shutdown(int sig)
{
    (void)sig;
    running = 0;
}

static void wake_all_waiters(void)
{
    // We are broadcasting to all cond wait, so we need to hold + release in the end the assosiated mutex
    pthread_mutex_lock(producer_fifo->mut);
    pthread_cond_broadcast(producer_fifo->notEmpty);
    pthread_cond_broadcast(producer_fifo->notFull);
    pthread_mutex_unlock(producer_fifo->mut);

    pthread_mutex_lock(&monitor_mut);
    pthread_cond_broadcast(&monitor_tick);
    pthread_mutex_unlock(&monitor_mut);
}

int main(void){
    struct sigaction sa;
    pthread_t pro, con, mon;

    producer_fifo = queueInit();
    if (producer_fifo == NULL)
        return 1;
    dataMutexInit();

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); // ctrl + C
    sigaction(SIGTERM, &sa, NULL); // kill signal

    // Create threads
    pthread_create(&pro, NULL, producer, NULL);
    pthread_create(&con, NULL, consumer, NULL);
    pthread_create(&mon, NULL, monitor, NULL);

    while (running) {
        pause();
    }

    // Wake up threads stuck, waiting for a condition signal
    wake_all_waiters();

    // Join threads
    pthread_join(pro, NULL);
    pthread_join(con, NULL);
    pthread_join(mon, NULL);

    queueDelete(producer_fifo);
    dataMutexDelete();
    wsDestroyWrapper();

    printf("Program ended gracefully!!\n");

    return 0;
}