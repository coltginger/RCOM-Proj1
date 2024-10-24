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
#define RR(n)  ( 0xAA | n )
#define REJ(n) ( 0x54 | n )
#define DISC 0x0B
#define ESC  0x7D

// MISC
typedef enum {
    START,
    START_FLAG, 
    A, 
    C, 
    BCC, 
    END_FLAG,
    CHECK,
    END
} STATE;

STATE state = START; 

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int retransmissionLimit;
int retransmissionTotalCount = 0;
int retransmissionCurCount; 
int timeout; 
int I_number = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    retransmissionCurCount++;
    retransmissionTotalCount++;
    state = START;
    

    printf("Alarm #%d\n", retransmissionTotalCount);
}


#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    state = START;
    retransmissionLimit = connectionParameters.nRetransmissions; 
    retransmissionCurCount = 0; 
    timeout = connectionParameters.timeout;
    (void) signal(SIGALRM, alarmHandler);
    if ( openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;
    while ( retransmissionLimit > retransmissionCurCount && state != END){
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
    if(retransmissionLimit <= retransmissionCurCount) return -1 ; 
    return 1;
}

int llopenTx(STATE *state){
    unsigned char byte; 
    switch(*state){
        case START:
            char* bytes = {FLAG, A_Tx, SET, A_Tx ^ SET, FLAG} ;
            if(writeBytesSerialPort(bytes,5)== -1 ) return -1;
            state = START_FLAG;
        break;
        case START_FLAG:
            if(readByteSerialPort(&byte) > 0){
                if(byte == FLAG){
                    state =A;
                }
            }
        case A:
            if(readByteSerialPort(&byte)> 0 ){
                if(byte == A_Rx ){
                    state = C;
                }
            }
        break;
        case C:
            if(readByteSerialPort(&byte) >0 ){
                if(byte == UA){
                    state = BCC;
                }
            }
        case BCC:
            if(readByteSerialPort(&byte) > 0){
                if(byte == UA ^ A_Rx){ 
                     state = END_FLAG;
                }
            }
        break;
        case END_FLAG:
            if(readByteSerialPort(&byte) > 0){
                if(byte == FLAG){
                    state = END;
                }
            }
        break;
    }
}

int llopenRx(STATE *state){
    unsigned char byte; 
    switch(*state){
        case START:
            if(readByteSerialPort(&byte)> 0){
                if(byte == FLAG){
                    state= START_FLAG;
                }
            }
        break;
        case START_FLAG:
            if(readByteSerialPort(&byte)> 0){
                if(byte == A_Tx ){
                    state = A;
                }
            }
        break;
        case A: 
            if(readByteSerialPort(&byte)> 0){
                if(byte == SET ){
                    state = C;
                }
            }
        break;
        case C:
            if(readByteSerialPort(&byte)> 0){
                if(byte == A_Tx ^ SET){
                    state = BCC;
                }
            }
        break;
        case BCC: 
            if(readByteSerialPort(&byte)> 0){
                if(byte == FLAG){
                    state = END_FLAG;
                }
            }
        break; 
        case END_FLAG:
            char* bytes = {FLAG, A_Rx,UA,A_Rx ^ UA,FLAG };
            if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
            state = END;
        break;
        
    }
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{   
    int frameSize = bufSize + 6;
    retransmissionCurCount = 0; 
    state = START; 
    (void) signal(SIGALRM, alarmHandler);
    unsigned char *frame = malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = A_Tx;
    frame[2] = I_number << 7; // I(0) / I(1)
    frame[3] = A_Tx ^ SET; // BCC1

    unsigned char bcc2 = 0;
    

    int k = 4;
    for (int i = 0; i < bufSize; i++)
    {
        if (buf[i] == FLAG || buf[i] == ESC)
        {   
            frameSize++;
            frame = realloc(frame, frameSize);
            frame[k++] = ESC;
            frame[k++] = buf[i] ^ 0x20;
            
            continue;
        }
        else{
            frame[k++] = buf[i];
        }
        bcc2 ^= buf[i];
        frameSize++;

    }

    frame[k++] = bcc2;
    frame[k++] = FLAG;
    unsigned char byte; 
    int success; 
    while( retransmissionLimit > retransmissionCurCount && state != END){
        if( alarmEnabled == FALSE){
            alarm(timeout);
            alarmEnabled = TRUE;
        }
        switch (state)
        {
        case START:
            if(writeBytesSerialPort(frame, frameSize)< 0 ) return -1 ;
            state = START_FLAG;
            break;
        
        case START_FLAG:
            if(readByteSerialPort(&byte) < 0 ) return -1 ; 
            if(byte == FLAG){
                state = A;
            }
            break;
        case A:
            if(readByteSerialPort(&byte) < 0 ) return -1 ; 
            if(byte ==  A_Rx){
                state = C;
            } 
            break;
        case C: 
            if(readByteSerialPort(&byte) < 0 ) return -1 ; 
            if(byte == RR(I_number)){
                success = TRUE; 
                state = BCC;
            }
            else if (byte == REJ(I_number))
            {
                success = FALSE;
                state = BCC;
            }
            break;
        case BCC: 
            if(readByteSerialPort(&byte) < 0) return -1 ; 
            if( (success && byte == A_Rx ^ RR(I_number)) || ( !success && byte == A_Rx ^ REJ(I_number)))  {
                state = END_FLAG; 
            }
            break;
        case END_FLAG: 
            if(readByteSerialPort(&byte) < 0 ) return -1; 
            if(byte == FLAG){
                state = CHECK;
            }
            break;
        case CHECK: 
            if(success){
                state = END;
            }
            else{
                state = START; 
                alarm(timeout); // restart timer in case of rej resend
            }
            break;
        }  
    }
    alarm(0);
    if(retransmissionLimit <= retransmissionCurCount) return -1 ; 
    I_number = !I_number; // change between I frame numbers (0/1)
    return 1;

}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    state = START;
    unsigned char byte; 
    while(state != END){
        switch (state)
        {
        case START:
            if(readByteSerialPort(&byte) < 0) return -1; 
            if(byte != FLAG){
                state = START; 
            }
            else{
                if(byte )
            } 
            break;
        
        
        }
    }

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
