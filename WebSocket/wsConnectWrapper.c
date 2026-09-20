#include "wsConnectWrapper.h"

int wsConnectWrapper(void){
    if (context != NULL) {
        lws_context_destroy(context);
        context = NULL;
    }

    if (wsi != NULL) {
        wsi = NULL;
    }

    int code1 = configContext();
    if (code1 != 0)
        return code1;

    configConnection();
    int code2 = ws_connect();

    return code1 || code2;
}

int wsDestroyWrapper(void){
    if (wsi != NULL) {
        lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, NULL, 0);
        wsi = NULL;
    }

    if (context != NULL) {
        lws_context_destroy(context);
        context = NULL;
    }

    return 0;
}