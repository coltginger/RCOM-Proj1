// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

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
        FILE* file = fopen(filename, "rb");
        if (file == NULL)
        {
            printf("Failed to open file\n");
            exit(-1);
        }

        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
    }
    else if(linkLayerRole == LlRx){
        // TODO
    }
    else{
        printf("Invalid role\n");
        exit(-1);
    }
}

unsigned char* makeControlPacket(const int controlField, int fileSize, const char* fileName){

    int L1 = sizeof(fileSize);
    int L2 = strlen(fileName);
    unsigned char *controlPacket = malloc(5+L1+L2);

    int k = 0;

    // Tamanho do Ficheiro
    controlPacket[k++] = controlField;     // C
    controlPacket[k++] = 0x00;             // T1
    controlPacket[k++] = L1;               // L1

    for(int i = 0; i < L1; i++){           // V1
        controlPacket[3+L1-i-1] = fileSize & 0xFF;
        fileSize >>= 8;
    }
    k += L1;

    // Nome do Ficheiro
    controlPacket[k++] = 0x01;             // T2
    controlPacket[k++] = L2;               // L2
    memcpy(controlPacket+k, fileName, L2); // V2

    return controlPacket;
}


unsigned char* makeDataPacket(unsigned int sequenceNumber, int dataSize, unsigned char* data){

    int L2 = dataSize >> 8 & 0xFF;
    int L1 = dataSize & 0xFF;
    unsigned char *dataPacket = malloc(4+dataSize);

    int k = 0;
    dataPacket[k++] = 0x02;                      // C
    dataPacket[k++] = sequenceNumber;            // S
    dataPacket[k++] = L2;                        // L2
    dataPacket[k++] = L1;                        // L1

    memcpy(dataPacket+k, data, (256*L2) + L1);   // P

    return dataPacket;
}

unsigned char* makeData(FILE *file, int fileSize){
    unsigned char* data = malloc(sizeof(unsigned char) * fileSize);
    fread(data, sizeof(unsigned char), fileSize, file);
    return data;
}

