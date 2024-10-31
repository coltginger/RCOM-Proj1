// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned char* makeControlPacket(const int controlField, int fileSize, const char* fileName, int* packetSize){

    int L1 = sizeof(fileSize);
    int L2 = strlen(fileName);
    // printf("file name size: %d\n",L2);
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

    *packetSize = 5+L1+L2;
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

    if (llopen(connectionParameters) == -1)
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

        int pos = ftell(file);
        fseek(file,0L,SEEK_END);
        long int fileSize = ftell(file)-pos; // wont a long be cut off by makeControlPacket (cast to int)
        fseek(file,0L,SEEK_SET);

        int packetSize;
        unsigned char* startControlPacket = makeControlPacket(1, fileSize, filename, &packetSize);

        if (llwrite(startControlPacket, packetSize) == -1)
        {
            printf("Failed to send control packet\n");
            exit(-1);
        }

        unsigned char sequenceNumber = 0;
        unsigned char* data = makeData(file, fileSize);
        int i = fileSize;
        while(i > 0)
        {   
            
            
            int currentDataSize = (i > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : i;
            unsigned char currentData[currentDataSize];
            memcpy(currentData, data, currentDataSize);

            unsigned char* dataPacket = makeDataPacket(sequenceNumber, currentDataSize, currentData);

            if (llwrite(dataPacket, currentDataSize+4) == -1)
            {
                printf("Failed to send data packet\n");
                exit(-1);
            }
            sequenceNumber++;
            i -= currentDataSize;
            data += currentDataSize;
            free(dataPacket);
        }

        unsigned char* endControlPacket = makeControlPacket(3, fileSize, filename, &packetSize);
        if(llwrite(endControlPacket, packetSize) == -1)
        {
            printf("Failed to send end control packet\n");
            exit(-1);
        }
        
    }
    else if(linkLayerRole == LlRx){
        FILE* file = fopen(filename, "wb");
        if (file == NULL)
        {
            printf("Failed to open file\n");
            exit(-1);
        }
        int loop = TRUE; 
        while(loop){
            printf("in loop\n");
            unsigned char packet[MAX_PAYLOAD_SIZE + 4];
            int bytes = llread(packet);
            if(bytes == -1){
                printf("Failed to read\n");
                exit(-1);
            }
            switch (packet[0])
            {
            case 1:{
                unsigned char fileSizeSize = packet[2];
                long int fileSize = 0; 
                for(int i = 0; i < fileSizeSize;i++){
                    fileSize = (fileSize << 8) | packet[3+i];
                }
                unsigned char fileNameSize = packet[4+fileSizeSize];
                //printf("file name size : %d",fileNameSize);
                unsigned char *fileName = malloc(fileNameSize+1);
                memcpy(fileName,packet+5+fileSizeSize,fileNameSize);
                fileName[fileNameSize] = '\0'; 
                printf("Recieving file(%s) of size:%ld\n",fileName,fileSize);
                free(fileName);
            }
                
                
                break;
            
            case 2: {
                int size = 256*packet[2] + packet[3];
                fwrite(packet+4,sizeof(unsigned char),size,file);
            
            } break;
                
            case 3: {
                unsigned char fileSizeSize = packet[2];
                long int fileSize = 0; 
                for(int i = 0; i < fileSizeSize;i++){
                    fileSize = (fileSize << 8) | packet[3+i];
                }
                unsigned char fileNameSize = packet[4+fileSizeSize];
                //printf("file name size : %d",fileNameSize);
                unsigned char *fileName = malloc(fileNameSize+1);
                memcpy(fileName,packet+5+fileSizeSize,fileNameSize);
                fileName[fileNameSize] = '\0'; 
                printf("Recieving file(%s) of size:%ld\n",fileName,fileSize);
                free(fileName);
                loop = FALSE;
            }
             
            break; 
            }
        }
        
    }
    else{
        printf("Invalid role\n");
        exit(-1);
    }
    printf("outside\n");
    llclose(1);

}

