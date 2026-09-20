#include "websocketHelpers.h"
#include "queue.h"
#include "threads.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct lws_protocols protocols[] = {
    {
        .name = "jetstream-protocol",
        .callback = ws_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    { .name = NULL, .callback = NULL }
};
struct lws_context_creation_info info;
struct lws_context *context = NULL;
struct lws_client_connect_info ccinfo;
struct lws *wsi = NULL;
int reconnectSleepTimes[5] = {1, 2, 5, 10, 20};

int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
    static int currentReconnectSleepTime = 0;
    (void)wsi;
    (void)len;
    (void)user;

    switch (reason){
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            currentReconnectSleepTime = 0;
            printf("Websocket Connection Established!\n");
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            pthread_mutex_lock(producer_fifo->mut);

            // Check if queue is full
            while (producer_fifo->full) {
                pthread_cond_wait(producer_fifo->notFull, producer_fifo->mut);
            }

            queueAdd(producer_fifo, (const char *)in, len);
            pthread_mutex_unlock(producer_fifo->mut);
            pthread_cond_signal(producer_fifo->notEmpty);
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (in != NULL){
                printf("CONNECTION ERROR: %.*s\n", (int)len, (char *)in); // no '\0', this print prints exactly len chars
            }
            else{
                printf("CONNECTION ERROR: UNKNOWN REASON\n");
            }
            if (running) {
                printf("Reconnecting in %d second(s). . .\n", reconnectSleepTimes[currentReconnectSleepTime]);
                sleep(reconnectSleepTimes[currentReconnectSleepTime]);
                if(currentReconnectSleepTime < 4){
                    currentReconnectSleepTime++;
                }
                ws_connect();
                break;
            }
            printf("WS connection gracefully closing\n");
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            if (in != NULL){
                printf("CLIENT CLOSED: %.*s\n", (int)len, (char *)in);
            }
            else{
                printf("CLIENT CLOSED: UNKNOWN REASON\n");
            }
            if (running) {
                printf("Reconnecting in %d second(s). . .\n", reconnectSleepTimes[currentReconnectSleepTime]);
                sleep(reconnectSleepTimes[currentReconnectSleepTime]);
                if(currentReconnectSleepTime < 4){
                    currentReconnectSleepTime++;
                }
                ws_connect();
                break;
            }
            printf("WS connection gracefully closing\n");
            break;
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            if (in != NULL){
                printf("PEER INITIATED CLOSE: %.*s\n", (int)len, (char *)in);
            }
            else{
                printf("PEER INITIATED CLOSE: UNKNOWN REASON\n");
            }
            break;
        default:
            break;
    }

    return 0;
}

int configContext(){
    // Configure context
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    // Create context
    context = lws_create_context(&info);

    if (context == NULL)
        return 1;

    return 0;
}

void configConnection(){
    // Configure connection
    memset(&ccinfo, 0, sizeof(ccinfo));

    ccinfo.context = context;
    ccinfo.address = "jetstream1.us-east.bsky.network";
    ccinfo.port = 443;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    ccinfo.path = "/subscribe?wantedCollections=app.bsky.feed.post"; // Endpoint where data are posted
    ccinfo.host = "jetstream1.us-east.bsky.network"; // HOSTNAME
    ccinfo.origin = "https://bsky.social";
    ccinfo.protocol = protocols[0].name;
    ccinfo.ietf_version_or_minus_one = -1; // Use the latest version
}

int ws_connect(){
    wsi = lws_client_connect_via_info(&ccinfo);

    if (wsi == NULL){
        printf("lws_client_connect_via_info failed");
        return 1;
    }

    return 0;
}