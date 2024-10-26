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


}
