#ifndef WEBSOCKETHELPERS_H
#define WEBSOCKETHELPERS_H

#include <libwebsockets.h>

extern struct lws_protocols protocols[];
extern struct lws_context_creation_info info;
extern struct lws_context *context;
extern struct lws_client_connect_info ccinfo;
extern struct lws *wsi;
extern int reconnectSleepTimes[5];

int configContext(); // Configure and create Context
void configConnection();// Configure connection
int ws_connect();
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

#endif