// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <unistd.h>
#include <signal.h>
#include <stdio.h>

// MACROS
#define FLAG 0x7E
#define A_Tx 0x03
#define A_Rx 0x01
#define SET  0x03
#define UA   0x07
#define RR0  0xAA
#define RR1  0xAB
#define REJ0 0x54
#define REJ1 0x55
#define DISC 0x0B

// MISC
enum {
    START,
    SET_START_FLAG, 
    SET_A, 
    SET_C, 
    SET_BCC, 
    SET_END_FLAG, 
    UA_START_FLAG, 
    UA_A, 
    UA_SET_C, 
    UA_SET_BCC, 
    UA_SET_END_FLAG, 
    END
} OPEN_STATE

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int retransmissions;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    retransmissions--;
    state = START;
    

    printf("Alarm #%d\n", alarmCount);
}


#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    int retransmissions = connectionParameters.nRetransmissions; 
    int timeout = connectionParameters.timeout;
    OPEN_STATE state = START; 
    
    if ( openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;
    while (retransmissions > 0 && state != END){
        if( alarmEnabled == FALSE){
            alarm(timeout);
            alarmEnabled = TRUE;
        }
        switch(connectionParameters.role){
            case L1Tx:
                if( llopenTx(&state)) return -1;
            break; 
            case L1Rx:
                if( llopenRx(&state)) return -1;
            break;
            default:
            return -1;
        }
    }


    return 1;
}

int llopenTx(OPEN_STATE *state){
    unsigned char byte; 
    switch(state){
        case START:
            writeBytesSerialPort({FLAG},1);
            state = SET_START_FLAG;
        break;
        case SET_START_FLAG:
            writeBytesSerialPort({A_Tx},1);
            state = SET_A;
        break;
        case SET_A: 
            writeBytesSerialPort({SET},1);
            state = SET_C;
        break;
        case SET_C:
            writeBytesSerialPort({A_Tx ^ SET},1);
            state = SET_BCC;
        break;
        case SET_BCC: 
            writeBytesSerialPort({FLAG},1);
            state = SET_END_FLAG;
        break; 
        case SET_END_FLAG:
            if(readByteSerialPort(&byte)){

            }
        break;
        case UA_START_FLAG:
            if(readByteSerialPort()){

            }
        break;
        case UA_A:
            if(){

            }
        break;
        case UA_SET_C:
            if(){

            }
        break;
        case UA_SET_BCC:
            if(){

            }
        break;

    UA_START_FLAG, 
    UA_A, 
    UA_SET_C, 
    UA_SET_BCC, 
    UA_SET_END_FLAG, 
    END


    }
}

int llopenRx(OPEN_STATE state){

}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
