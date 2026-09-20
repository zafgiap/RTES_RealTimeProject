#define _POSIX_C_SOURCE 200809L

#include "threads.h"
#include "wsConnectWrapper.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

pthread_mutex_t monitor_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t monitor_tick = PTHREAD_COND_INITIALIZER;
int monitor_tick_pending = 0;

extern volatile sig_atomic_t running;

static int read_cpu_counters(unsigned long long *total, unsigned long long *idle){
    FILE *stat_file;
    char line[256];
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle_time;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long guest;
    unsigned long long guest_nice;

    stat_file = fopen("/proc/stat", "r");
    if (stat_file == NULL)
        return -1;

    if (fgets(line, sizeof(line), stat_file) == NULL) {
        fclose(stat_file);
        return -1;
    }

    fclose(stat_file);

    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle_time, &iowait, &irq, &softirq,
               &steal, &guest, &guest_nice) < 4)
        return -1;

    *total = user + nice + system + idle_time + iowait + irq + softirq +
        steal;
    *idle = idle_time + iowait;
    return 0;
}

static void monitor_timer_callback(union sigval value)
{
    (void)value;

    pthread_mutex_lock(&monitor_mut);
    monitor_tick_pending = 1;
    pthread_cond_signal(&monitor_tick);
    pthread_mutex_unlock(&monitor_mut);
}

void *producer (void *arg){
    (void)arg;

    int code = wsConnectWrapper();
    if (code){
        return NULL;
    }

    while (running){
        lws_service(context, 0);
    }

    return NULL;
}

void *consumer (void *arg){
    (void)arg;
    char *data = NULL;

    while (running){
        pthread_mutex_lock(producer_fifo->mut);
        while (producer_fifo->empty && running) {
            pthread_cond_wait(producer_fifo->notEmpty, producer_fifo->mut);
        }
        if (!running) {
            pthread_mutex_unlock(producer_fifo->mut);
            break;
        }
        queueDel(producer_fifo, &data);
        pthread_mutex_unlock(producer_fifo->mut);
        pthread_cond_signal(producer_fifo->notFull);

        if (data != NULL) {
            JSONparse(data);
            free(data);
            data = NULL;
        }
    }

    return NULL;

}

void *monitor(void *arg){
    (void)arg;
    struct sigevent event;
    struct itimerspec timer_spec;
    timer_t timer_id;
    FILE *log_file;
    unsigned long long previous_total;
    unsigned long long previous_idle;
    int commit_count;
    int identity_count;
    int account_count;
    int parsed_info_count;
    long queued_messages;
    double queue_percentage;
    double cpu_percentage;
    unsigned long long current_total;
    unsigned long long current_idle;
    struct timespec timestamp;
    unsigned long long total_delta;
    unsigned long long idle_delta;
    unsigned long long busy_delta;

    if (producer_fifo == NULL || data_mut == NULL)
        return NULL;

    // Configuring what event will happen when the timer hits
    event.sigev_notify = SIGEV_THREAD;
    event.sigev_notify_function = monitor_timer_callback;
    event.sigev_value.sival_ptr = NULL;
    event.sigev_notify_attributes = NULL;

    // Assigning the event to the timer that will be used
    if (timer_create(CLOCK_MONOTONIC, &event, &timer_id) == -1)
        return NULL;

    // Configure and set timer (fire every 1 sec exactly)
    timer_spec.it_value.tv_sec = 1;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 1;
    timer_spec.it_interval.tv_nsec = 0;

    if (timer_settime(timer_id, 0, &timer_spec, NULL) == -1) {
        timer_delete(timer_id);
        return NULL;
    }

    remove("metrics_log.txt");
    log_file = fopen("metrics_log.txt", "w");
    if (log_file == NULL) {
        timer_delete(timer_id);
        return NULL;
    }

    fprintf(log_file,
            "Seconds,Nanoseconds,Commit_Count,Identity_Count,"
            "Account_Count,Info_Count,Buffer_Occupancy_Pct,CPU_Pct\n");
    fflush(log_file);

    if (read_cpu_counters(&previous_total, &previous_idle) != 0) {
        fclose(log_file);
        timer_delete(timer_id);
        return NULL;
    }

    while (running) {
        pthread_mutex_lock(&monitor_mut);
        monitor_tick_pending = 0;
        while (!monitor_tick_pending && running){
            pthread_cond_wait(&monitor_tick, &monitor_mut);
        }
        
        // When monitor_tick_pending is set to 1, thread wakes up and we immediately measure the wake up time
        clock_gettime(CLOCK_REALTIME, &timestamp);
        
        if (!running) {
            pthread_mutex_unlock(&monitor_mut);
            break;
        }
        pthread_mutex_unlock(&monitor_mut);

        // Counter variables management
        pthread_mutex_lock(data_mut);
        commit_count = commit;
        identity_count = identity;
        account_count = account;
        parsed_info_count = info_count;
        commit = 0;
        identity = 0;
        account = 0;
        info_count = 0;
        pthread_mutex_unlock(data_mut);

        // Circular buffer usage calculation
        pthread_mutex_lock(producer_fifo->mut);
        if (producer_fifo->full) {
            queued_messages = QUEUESIZE;
        } else if (producer_fifo->empty) {
            queued_messages = 0;
        } else if (producer_fifo->tail > producer_fifo->head) {
            queued_messages = producer_fifo->tail - producer_fifo->head;
        } else {
            queued_messages = (QUEUESIZE - producer_fifo->head) + producer_fifo->tail;
        }
        pthread_mutex_unlock(producer_fifo->mut);
        queue_percentage = 100.0 * (double)queued_messages / QUEUESIZE;

        // CPU usage calculation
        if (read_cpu_counters(&current_total, &current_idle) == 0 &&
            current_total > previous_total) {
            total_delta = current_total - previous_total;
            idle_delta = current_idle >= previous_idle ?
                current_idle - previous_idle : 0;
            busy_delta = total_delta > idle_delta ?
                total_delta - idle_delta : 0;
            cpu_percentage = 100.0 * (double)busy_delta / total_delta;
            previous_total = current_total;
            previous_idle = current_idle;
        } else {
            cpu_percentage = 0.0;
        }

        fprintf(log_file, "%lld,%ld,%d,%d,%d,%d,%.2f,%.2f\n",
                (long long)timestamp.tv_sec, timestamp.tv_nsec,
                commit_count, identity_count, account_count, parsed_info_count,
                queue_percentage, cpu_percentage);
        fflush(log_file);

    }

    fclose(log_file);
    timer_delete(timer_id);
    return NULL;

}