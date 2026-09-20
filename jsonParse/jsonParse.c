#include "queue.h"
#include "jsonParse.h"

#include <stdlib.h>
#include <string.h>

int commit = 0;
int identity = 0;
int account = 0;
int info_count = 0;
pthread_mutex_t *data_mut = NULL;

void JSONparse(char *data){

    cJSON *parent = cJSON_Parse(data);
    if (parent == NULL){
        return;
    }

    cJSON *key = cJSON_GetObjectItem(parent, "kind");

    if (cJSON_IsString(key) && key->valuestring != NULL){
        if (strcmp(key->valuestring, "commit") == 0){
            pthread_mutex_lock(data_mut);
            commit++;
            pthread_mutex_unlock(data_mut);
        }
        else if (strcmp(key->valuestring, "identity") == 0){
            pthread_mutex_lock(data_mut);
            identity++;
            pthread_mutex_unlock(data_mut);
        }
        else if (strcmp(key->valuestring, "account") == 0){
            pthread_mutex_lock(data_mut);
            account++;
            pthread_mutex_unlock(data_mut);
        }
        else if (strcmp(key->valuestring, "info") == 0){
            pthread_mutex_lock(data_mut);
            info_count++;
            pthread_mutex_unlock(data_mut);
        }
        
    }

    cJSON_Delete(parent);
}

void dataMutexInit(){
    data_mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init(data_mut, NULL);
}

void dataMutexDelete(){
    pthread_mutex_destroy(data_mut);
    free(data_mut);
}