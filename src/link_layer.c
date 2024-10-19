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
#define ESC  0x7D

// MISC
typedef enum {
    START,
    SET_START_FLAG, 
    SET_A, 
    SET_C, 
    SET_BCC, 
    SET_END_FLAG, 
    UA_START_FLAG, 
    UA_A, 
    UA_C, 
    UA_BCC, 
    UA_END_FLAG, 
    END
} OPEN_STATE;

OPEN_STATE state = START; 

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int retransmissions;
int alarmCount = 0; 

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
    state = START;
    int retransmissions = connectionParameters.nRetransmissions; 
    int timeout = connectionParameters.timeout;
    
    if ( openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;
    while (retransmissions > 0 && state != END){
        if( alarmEnabled == FALSE){
            alarm(timeout);
            alarmEnabled = TRUE;
        }
        switch(connectionParameters.role){
            case  LlTx:
                if( llopenTx(&state)) return -1;
            break; 
            case LlRx:
                if( llopenRx(&state)) return -1;
            break;
            default:
            return -1;
        }
    }
    alarm(0);
    if(retransmissions >0 ) return -1 ; 
    return 1;
}

int llopenTx(OPEN_STATE *state){
    unsigned char byte; 
    switch(*state){
        case START:
            char* bytes = {FLAG, A_Tx, SET, A_Tx ^ SET, FLAG} ;
            if(writeBytesSerialPort(bytes,5)== -1 ) return -1;
            state = SET_END_FLAG;
        break;
        case SET_END_FLAG:
            if(readByteSerialPort(&byte)){
                if(byte == FLAG){
                    state = UA_START_FLAG;
                }
            }
        case UA_START_FLAG:
            if(readByteSerialPort(&byte)){
                if(byte == A_Rx ){
                    state = UA_A;
                }
            }
        break;
        case UA_A:
            if(readByteSerialPort(&byte)){
                if(byte == UA){
                    state = UA_C;
                }
            }
        case UA_C:
            if(readByteSerialPort(&byte)){
                if(byte == UA ^ A_Rx){ 
                     state = UA_BCC;
                }
            }
        break;
        case UA_BCC:
            if(readByteSerialPort(&byte)){
                if(byte == FLAG){
                    state = END;
                }
            }
        break;
    }
}

int llopenRx(OPEN_STATE *state){
    unsigned char byte; 
    switch(*state){
        case START:
            if(readByteSerialPort(&byte)){
                if(byte == FLAG){
                    state= SET_START_FLAG;
                }
            }
        break;
        case SET_START_FLAG:
            if(readByteSerialPort(&byte)){
                if(byte == A_Tx ){
                    state = SET_A;
                }
            }
        break;
        case SET_A: 
            if(readByteSerialPort(&byte)){
                if(byte == SET ){
                    state = UA_A;
                }
            }
        break;
        case SET_C:
            if(readByteSerialPort(&byte)){
                if(byte == A_Tx ^ SET){
                    state = SET_BCC;
                }
            }
        break;
        case SET_BCC: 
            if(readByteSerialPort(&byte)){
                if(byte == FLAG){
                    state = SET_END_FLAG;
                }
            }
        break; 
        case SET_END_FLAG:
            if(readByteSerialPort(&byte)){
                if(byte == FLAG){
                    state= UA_START_FLAG;
                }
            }
        break;
        case UA_START_FLAG:
            char* bytes = {FLAG, A_Rx,UA,A_Rx ^ UA,FLAG };
           if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
           state = END;
        break;
    }
}



int llopenRx(OPEN_STATE state){

}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    unsigned char *frame = malloc(bufSize + 6);
    frame[0] = FLAG;
    frame[1] = A_Tx;
    frame[2] = SET;
    frame[3] = A_Tx ^ SET; // BCC1
    memcpy(frame + 4, buf, bufSize);

    unsigned char bcc2 = 0;
    for (int i = 0; i < bufSize; i++)
    {
        bcc2 ^= buf[i];
    }

    int k = 4;
    for (int i = 0; i < bufSize; i++)
    {
        if (buf[i] == FLAG || buf[i] == ESC)
        {
            frame[k++] = ESC;
            frame[k++] = buf[i] ^ 0x20;
            continue;
        }
        frame[k++] = buf[i];
    }

    frame[k++] = bcc2;
    frame[k++] = FLAG;

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    

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
