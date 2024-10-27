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

unsigned char* makeControlPacket(const int controlField, int length, const char* fileName){

    int L1 = sizeof(length);
    int L2 = strlen(fileName);
    unsigned char *controlPacket = malloc(5+L1+L2);

    int k = 0;
    controlPacket[k++] = controlField;
    controlPacket[k++] = 0x00;
    controlPacket[k++] = L1;

    for(int i = 0; i < L1; i++){
        controlPacket[3+L1-i-1] = length & 0xFF;
        length >>= 8;
    }
    k += L1;
    controlPacket[k++] = 0x01;
    controlPacket[k++] = L2;

    memcpy(controlPacket+k, fileName, L2);

    return controlPacket;
}
