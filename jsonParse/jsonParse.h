#ifndef JSON_PARSE_H
#define JSON_PARSE_H

#include <pthread.h>
#include <string.h>
#include <cjson/cJSON.h>

extern int commit;
extern int identity;
extern int account;
extern int info_count;

extern pthread_mutex_t *data_mut;

void dataMutexInit();
void dataMutexDelete();
void JSONparse(char *data);

#endif