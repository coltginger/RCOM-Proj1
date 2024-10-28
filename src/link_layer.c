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
#define I(n)   (n << 7)
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
    DATA,
    END
} STATE;

STATE state = START; 

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
// vairiables initialized in open
int retransmissionLimit;
int timeout; 
LinkLayerRole role; 
// stats
int retransmissionTotalCount = 0;
int timeoutCount = 0; 
int frameSentCount = 0; 
int frameRcvSuccessfullyCount = 0; 

int retransmissionCurCount; 
// I frame number 
int I_number = 0; // current

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    retransmissionCurCount++;
    retransmissionTotalCount++;
    timeoutCount++;
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
    role = connectionParameters.role; 
    retransmissionCurCount = 0; 
    timeout = connectionParameters.timeout;
    (void) signal(SIGALRM, alarmHandler);
    if ( openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;
    while ( retransmissionLimit > retransmissionCurCount && state != END){
        if( alarmEnabled == FALSE){
            alarm(timeout);
            alarmEnabled = TRUE;
        }
        switch(role){
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
    alarmEnabled = FALSE; 
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
            frameSentCount++;
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
                    frameRcvSuccessfullyCount++; 
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
                    frameRcvSuccessfullyCount++;
                }
            }
        break; 
        case END_FLAG:
            char* bytes = {FLAG, A_Rx,UA,A_Rx ^ UA,FLAG };
            if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
            state = END;
            frameSentCount++;
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
    frame[2] = I(I_number); 
    frame[3] = A_Tx ^ SET; // BCC1

    unsigned char bcc2 = 0;
    

    int k = 4;
    for (int i = 0; i < bufSize; i++)
    {
        if (buf[i] == FLAG || buf[i] == ESC) // byte stuffing for the data
        {   
            frameSize++;
            frame = realloc(frame, frameSize);
            frame[k++] = ESC;
            bcc2 ^= ESC;
            frame[k] = buf[i] ^ 0x20;
            
        }
        else{
            frame[k] = buf[i];
        }
        bcc2 ^= frame[k]; 
        k++;
    }
    if(bcc2 == ESC || bcc2 == FLAG){ // byte stuffing for the bcc2
        frameSize++;
        frame = realloc(frame, frameSize);
        frame[k++] = ESC;
        frame[k++] = bcc2 ^ 0x20;
    }
    else{
        frame[k++] = bcc2;
    }
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
        case START: // refactor later to make write seperate from reads 
            if(writeBytesSerialPort(frame, frameSize)< 0 ) return -1 ;
            state = START_FLAG;
            frameSentCount++;
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
            if(byte == RR(!I_number)){
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
            if( (success && byte == A_Rx ^ RR(!I_number)) || ( !success && byte == A_Rx ^ REJ(I_number)))  {
                state = END_FLAG; 
                frameRcvSuccessfullyCount++; 
            }
            break;
        case END_FLAG: 
            if(readByteSerialPort(&byte) < 0 ) return -1; 
            if(byte == FLAG){
                if(success){
                state = END;
            }
            else{
                state = START; 
                retransmissionTotalCount++;
                alarm(timeout); // restart timer in case of rej resend
            }
            }
            break;
        
        }  
    }
    // cleanup
    alarm(0);
    alarmEnabled=FALSE;
    free(frame); 
    if(retransmissionLimit <= retransmissionCurCount) return -1 ; 
    I_number = !I_number; // change between I frame numbers (0/1)
    return 1;

}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    unsigned char *frame = malloc(1); // very unoptimized / unneccessary buffer, might remove later
    int pos = 0; 
    state = START;
    unsigned char byte; 
    int duplicate = FALSE; 
    int escaped = FALSE; 
    int bcc2 = 0; 
    unsigned char c; 

    while(state != END){
        int bytes = readByteSerialPort(&byte); 
        if(bytes > 0){
            switch (state)
            {
            case START:
                if(byte == FLAG){
                    state = START_FLAG; 
                }
            break; 
            case START_FLAG: 
                if(byte == A_Tx){
                    state = A; 
                }
                if(byte == FLAG){
                    state = START_FLAG;
                }
                else{
                    state = START;
                }
            break; 
            case A: // add exit condition here 
                if(byte == I(I_number)){
                    state = C;
                    c = byte; 
                }
                else if(byte == I(!I_number)){
                    state = C;
                    c = byte; 
                    duplicate = TRUE;
                }
                else if(byte == FLAG){
                    state = START_FLAG;
                }
                else{
                    state = START;
                }
            break; 
            case C: 
                if(byte == A_Tx ^ c){
                    state = BCC; 
                }
                else if(byte == FLAG){
                    state = START_FLAG;
                }
                else{
                    state = START;
                }
            break; 
            case BCC: 
                if(byte != FLAG){
                    if(byte == ESC){
                        escaped = TRUE; 
                    }
                    else if(escaped){
                        frame[pos++] = byte ^ 0x20; 
                        bcc2 ^= byte ^ 0x20; 
                        realloc(frame, pos + 1);
                    }
                    else{
                        frame[pos++] = byte; 
                        bcc2 ^= byte; 
                        realloc(frame, pos + 1 );
                    }
                }
                if(byte ==FLAG){ // 
                    if(bcc2 == 0){ // dark magick to spare one more loop ( point of possible failure )
                        if(duplicate){
                            char* bytes = {FLAG, A_Rx,RR(I_number),A_Rx ^ RR(I_number),FLAG };
                            
                        }
                        else{
                            I_number = !I_number; // switch I frame
                            char* bytes = {FLAG, A_Rx,RR(I_number),A_Rx ^ RR(I_number),FLAG };
                            memcpy(packet, frame, pos - 1 ); 
                            
                        }
                        frameRcvSuccessfullyCount++;
                    }
                    else{
                        if(duplicate){
                            char* bytes = {FLAG, A_Rx,RR(I_number),A_Rx ^ RR(I_number),FLAG };
                        }
                        else{
                            char* bytes = {FLAG, A_Rx,REJ(I_number),A_Rx ^ REJ(I_number),FLAG };
                        }
                    }
                    if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
                    state = END; 
                    frameSentCount++;
                }
            }
            

        }
        
    }
    // cleanup
    free(frame);
    return pos - 1 ;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{   
    switch (role)
    {
    case LlTx:
        if(llcloseTx() < 0) return -1;
        break;
    case LlRx: 
        if(llcloseRx() < 0) return -1; 
    }
    if(showStatistics){
        printf("======================================================\n");
        printf("Total number of frames sent: %d\n",frameSentCount);
        printf("Number of frames retransmissioned: %d\n",retransmissionTotalCount);
        printf("in which %d were retransmissioned due to timeout",timeoutCount);
        printf("Total number of frames received successfully: %d\n",frameRcvSuccessfullyCount);
        printf("======================================================\n");
    }
    int clstat = closeSerialPort();
    return clstat;
}
int llcloseTx(){
    retransmissionCurCount = 0; 
    (void) signal(SIGALRM, alarmHandler);
    state = START;
    unsigned char byte; 
    while(retransmissionCurCount < retransmissionLimit && state != END){
        if( alarmEnabled == FALSE){
            alarm(timeout);
            alarmEnabled = TRUE;
            char* bytes = {FLAG, A_Tx,DISC,A_Tx ^ DISC,FLAG };
            if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
        }
        int bytes = readByteSerialPort(&byte);
        if(bytes > 0 ){
            switch (state)
            {
            case START:
                if(byte == FLAG){
                    state = START_FLAG; 
                }
                break;
            case START_FLAG: 
                if(byte == A_Rx){
                    state = A; 
                }
                else if ( byte == FLAG){
                    state = START_FLAG; 
                }
                else{
                    state = START;
                }
                break; 
            case A: 
                if(byte == DISC){
                    state = C;
                }
                else if(byte == FLAG){
                    state = START_FLAG; 
                }
                else {
                    state = START; 
                }
                break; 
            case C: 
                if(byte == A_Rx ^ DISC){
                    state = BCC; 
                }
                else if(byte == FLAG){
                    state = START_FLAG;
                }
                else{
                    state = START;
                }
                break;
            case BCC: 
                if(byte == FLAG){
                    state = END; 
                }
                else{
                    state = START; 
                }
            }
        }

    }
    
    // cleanup
    alarm(0);
    alarmEnabled=FALSE;
    if(retransmissionLimit <= retransmissionCurCount) return -1 ; 
    // send UA
    char* bytes = {FLAG, A_Tx,UA,A_Tx ^ UA,FLAG };
    if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
    return 1; 
}
int llcloseRx(){
    state = START;
    unsigned char byte; 
    int sndRound = FALSE; // if true, its checking the UA, else its checking the DISC
    while (state != END){
        int bytes = readByteSerialPort(&byte);
        if(bytes > 0 ){
            switch (state)
            {
            case START:
                if(byte == FLAG){
                    state = START_FLAG; 
                }
                else{
                    sndRound = FALSE; 
                }
                break;
            case START_FLAG: 
                if(byte == A_Tx){
                    state = A; 
                }
                else if ( byte == FLAG){
                    state = START_FLAG; 
                    sndRound = FALSE; 
                }
                else{
                    state = START;
                    sndRound = FALSE; 
                }
                break; 
            case A: 
                if((byte == DISC && !sndRound ) ||(byte == UA && sndRound ) ){
                    state = C;
                }
                else if(byte == FLAG){
                    state = START_FLAG; 
                    sndRound = FALSE; 
                }
                else {
                    state = START; 
                    sndRound = FALSE; 
                }
                break; 
            case C: 
                if((byte == A_Tx ^ DISC && !sndRound ) || (byte == A_Tx ^ UA && sndRound ) ){
                    state = BCC; 
                }
                else if(byte == FLAG){
                    state = START_FLAG;
                    sndRound = FALSE; 
                }
                else{
                    state = START;
                    sndRound = FALSE; 
                }
                break;
            case BCC: 
                if(byte == FLAG){
                    if(sndRound){
                        state = END;
                    }
                    else{
                        state = START; 
                        sndRound = TRUE; 
                        char* bytes = {FLAG, A_Rx,DISC,A_Rx ^ DISC,FLAG };
                        if(writeBytesSerialPort(bytes,5)== -1 ) return -1; 
                    }
                }
                else{
                    state = START; 
                    sndRound = FALSE;
                }
            }
        }
    }
    return 1; 
}
