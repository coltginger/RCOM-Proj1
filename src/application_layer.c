// Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    LinkLayerRole linkLayerRole;
    linkLayerRole = (strcmp(role, "tx") == 0) ? LlTx : LlRx;

    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.role = linkLayerRole;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    if (llopen(connectionParameters) < 0)
    {
        printf("Failed to establish connection\n");
        exit(-1);
    }

    if(linkLayerRole == LlTx){
        FILE *file = fopen(filename, "placeholder");
        if(file == NULL){
            printf("Failed to open file\n");
            exit(-1);
        }

    }
    else if(linkLayerRole == LlRx){
    }
    else{
        printf("Invalid role\n");
        exit(-1);
    }
}
