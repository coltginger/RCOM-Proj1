// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MACROS
#define FLAG 0x7E
#define A_Tx 0x03
#define A_Rx 0x01
#define SET 0x03
#define UA 0x07
#define RR(n) (0xAA | n)
#define REJ(n) (0x54 | n)
#define I(n) (n << 7)
#define DISC 0x0B
#define ESC 0x7D

// MISC
typedef enum
{
    START,
    START_FLAG,
    A,
    C,
    BCC,
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
int I_number = 0;
int incompleteIFrame = FALSE; // when in llopen, the exit condition is reading a correct header for I frame, in that case, llread must skip trying to read the header again for the first time

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    retransmissionCurCount++;
    retransmissionTotalCount++;
    timeoutCount++;
    printf("Timeout %d of %d\n", retransmissionCurCount,retransmissionLimit);
}

#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopenTx()
{

    retransmissionCurCount = 0;
    (void)signal(SIGALRM, alarmHandler);
    unsigned char byte;
    while (retransmissionLimit > retransmissionCurCount && state != END)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(timeout);
            alarmEnabled = TRUE;
            unsigned char sFrame[] = {FLAG, A_Tx, SET, A_Tx ^ SET, FLAG};
            if (writeBytesSerialPort(sFrame, 5) == -1)
                return -1;
            frameSentCount++;
            state = START; 
        }
        int bytes = readByteSerialPort(&byte);
        
        if (bytes > 0)
        {   
            //printf("byte: 0x%02x\n",byte);
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = START_FLAG;
                    //printf("flag\n");
                    
                }
                break;
            case START_FLAG:
                if (byte == A_Rx)
                {
                    state = A;
                    //printf("A\n");
                }
                else if (byte != FLAG)
                {
                    state = START;
                }
                break;
            case A:
                if (byte == UA)
                {
                    state = C;
                    //printf("C");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case C:
                if (byte == (A_Rx ^ UA))
                {
                    state = BCC;
                    //printf("bcc\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC:
                if (byte == FLAG)
                {
                    state = END;
                    frameRcvSuccessfullyCount++;
                    //printf("end\n");
                }
                else
                {
                    state = START;
                }
                break;
            default:
                return -1;
                break;
            }
        }
    }
    alarm(0);
    alarmEnabled = FALSE;
    //printf("retr:%d\n",retransmissionCurCount);
    if (retransmissionLimit <= retransmissionCurCount)
        return -1;
    return 1;
}

int llopenRx()
{
    int I_frame; // if true then its checking for I frame else checking for SET
    unsigned char byte;
    while (state != END)
    {
        int bytes = readByteSerialPort(&byte);
        if (bytes > 0)
        {   
            //printf("byte : 0x%02x\n",byte);
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    I_frame = FALSE;
                    state = START_FLAG;
                    //printf("flag\n");
                }
                break;
            case START_FLAG:
                if (byte == A_Tx)
                {
                    state = A;
                    //printf("A\n");
                }
                else if (byte != FLAG)
                {
                    state = START;
                }
                break;
            case A:
                if (byte == SET)
                {
                    state = C;
                    //printf("c\n");
                }
                else if (byte == I(I_number))
                {
                    I_frame = TRUE;
                    state = C;
                    //printf("i\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case C:
                if ((byte == (A_Tx ^ SET)) && !I_frame)
                {
                    state = BCC;
                    //printf("bcc\n");
                }
                else if(byte == (A_Tx ^ I(I_number)) && I_frame){
                    state = END;
                    incompleteIFrame = TRUE;
                    //printf("I complete\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC:
                if (byte == FLAG)
                {
                    //printf("con\n");
                    unsigned char sFrame[] = {FLAG, A_Rx, UA, A_Rx ^ UA, FLAG};
                    if (writeBytesSerialPort(sFrame, 5) == -1)
                        return -1;
                    state = START;
                    frameSentCount++;
                    frameRcvSuccessfullyCount++;
                    
                }
                else
                {
                    state = START;
                }
                break;
            default:
                return -1;
            }
        }
    }
    return 1;
}

int llopen(LinkLayer connectionParameters)
{
    state = START;
    retransmissionLimit = connectionParameters.nRetransmissions;
    role = connectionParameters.role;
    timeout = connectionParameters.timeout;
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        return -1;
    switch (role)
    {
    case LlTx:
        if (llopenTx() == -1)
            return -1;
        break;
    case LlRx:
        if (llopenRx() == -1)
            return -1;
        break;
    }
    return 1;
}
////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int frameSize = bufSize + 6;
    retransmissionCurCount = 0;
    state = START;
    (void)signal(SIGALRM, alarmHandler);
    unsigned char *frame = malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = A_Tx;
    frame[2] = I(I_number);
    frame[3] = A_Tx ^ I(I_number); // BCC1

    unsigned char bcc2 = 0;

    int k = 4;
    for (int i = 0; i < bufSize; i++)
    {
        if (buf[i] == FLAG || buf[i] == ESC) // byte stuffing for the data
        {
            frameSize++;
            frame = realloc(frame, frameSize);
            frame[k++] = ESC;
            
            frame[k] = buf[i] ^ 0x20;
            
        }
        else
        {
            frame[k] = buf[i];
            
        }
        
        k++;
        
    }
    for(int i = 0 ; i < bufSize; i++){
        bcc2  ^= buf[i];
    } 
    
    if (bcc2 == ESC || bcc2 == FLAG)
    { // byte stuffing for the bcc2
        frameSize++;
        frame = realloc(frame, frameSize);
        frame[k++] = ESC;
        frame[k++] = bcc2 ^ 0x20;
        //printf("bcc2 pre stuff:0x%02x\n", bcc2);
    }
    else
    {
        frame[k++] = bcc2;
        //printf("bcc2:0x%02x\n", bcc2);
    }
    
    frame[k++] = FLAG;
    unsigned char byte;
    int success;
    for(int i = 0; i < k; i++){
        //printf("0x%02x ",frame[i]);
    }
    //printf("\n");
    while (retransmissionLimit > retransmissionCurCount && state != END)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(timeout);
            alarmEnabled = TRUE;
            //printf("I number sent: 0x%02X\n",frame[2]);
            if (writeBytesSerialPort(frame, frameSize) < 0)
                return -1;
            frameSentCount++;
            state = START; 
        }
        int bytes = readByteSerialPort(&byte);
        if (bytes > 0)
        {   
            //printf("byte: 0x%02x\n",byte);
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = START_FLAG;
                    //printf("flag\n");
                }
                break;
            case START_FLAG:
                if (byte == A_Rx)
                {
                    state = A;
                    //printf("a\n");
                }
                else if (byte != FLAG)
                {
                    state = START;
                }
                break;
            case A:
                if (byte == RR(!I_number))
                {
                    success = TRUE;
                    state = C;
                    //printf("rr\n");
                }
                else if (byte == REJ(I_number))
                {
                    success = FALSE;
                    state = C;
                    //printf("rej\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case C:
                //printf("xor: 0x%02x\n",A_Rx ^ RR(!I_number));
                if ((success && byte == (A_Rx ^ RR(!I_number))) || (!success && byte == (A_Rx ^ REJ(I_number))))
                {
                    state = BCC;
                    //printf("bcc\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC:
                if (byte == FLAG)
                {
                    if (success)
                    {   
                        //printf("success\n");
                        state = END;
                    }
                    else
                    {   
                        //printf("resend\n");
                        state = START;
                        if (writeBytesSerialPort(frame, frameSize) < 0)
                            return -1;
                        frameSentCount++;
                        retransmissionTotalCount++;
                        alarm(timeout); // restart timer in case of rej resend
                    }
                    frameRcvSuccessfullyCount++;
                }
                else
                {
                    state = START;
                }
                break;
            default:
                return -1;
            }
        }
    }
    // cleanup
    alarm(0);
    alarmEnabled = FALSE;
    free(frame);
    if (retransmissionLimit <= retransmissionCurCount)
        return 0;
    I_number = !I_number; // change between I frame numbers (0/1)
    return frameSize;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char *frame = malloc(2*MAX_PAYLOAD_SIZE + 7); // very unoptimized / unneccessary buffer, might remove later
    int pos = 0;
    state = START;
    unsigned char byte;
    int duplicate = FALSE;
    int escaped = FALSE;
    unsigned char bcc2 = 0;
    unsigned char c;
    unsigned char *destuffedBuffer = malloc(2*MAX_PAYLOAD_SIZE + 7);
    while (state != END)
    {
        if (incompleteIFrame)
        {
            state = BCC;
            incompleteIFrame = FALSE;
            
            //printf("skipped to data\n");
        }
        int bytes = readByteSerialPort(&byte);
        if (bytes > 0)
        {   
            //printf("0x%02x ",byte);
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = START_FLAG;
                    //printf("\nflag\n");
                }
                break;
            case START_FLAG:
                if (byte == A_Tx)
                {
                    state = A;
                    //printf("\na\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case A:
                //printf("teste\n");
                //printf("simulated I: 0x%02x\n", I(I_number));
                if (byte == I(I_number))
                {
                    state = C;
                    c = byte;
                    //printf("\ncorrect i\n");
                }
                else if (byte == I(!I_number))
                {   
                    state = C;
                    c = byte;
                    duplicate = TRUE;
                    //printf("\ndup\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case C:
                if (byte == (A_Tx ^ c))
                {
                    state = BCC;

                    //printf("\nbcc\n");
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC:
                if(pos == 2*MAX_PAYLOAD_SIZE + 7){
                    state = START; 
                }
                if (byte != FLAG)
                {   
                    frame[pos++] = byte; 
                }
                if (byte == FLAG)
                {   
                    //printf("\nend of packet\n");
                    unsigned char sFrame[5];
                    int destuffedPointer = 0; 
                    /*
                    for( int i = 0; i < pos; i++){
                        printf("0x%02x ", frame[i]);
                    }
                    printf("\n");
                    */
                    
                    for(int i = 0 ; i < pos ; i++){ // destuffing 
                        if (frame[i] == ESC)
                        {
                            escaped = TRUE;
                        }
                        else if (escaped)
                        {   
                            destuffedBuffer[destuffedPointer++] = frame[i] ^ 0x20;
                            escaped = FALSE; 
                        }
                        else
                        {   
                            destuffedBuffer[destuffedPointer++] = frame[i];
                        }
                    }
                    /*
                    for(int i= 0 ; i < destuffedPointer; i++){
                        printf("0x%02x ",destuffedBuffer[i]);
                    }
                    */
                    //printf("\n");
                    for( int i = 0; i < destuffedPointer - 1; i++){
                        bcc2 ^= destuffedBuffer[i];
                    }
                    //printf("bcc2: 0x%02x\n",destuffedBuffer[destuffedPointer - 1]);
                    //printf("simulated bcc2: 0x%02x\n",bcc2);
                
                    if (bcc2 == destuffedBuffer[destuffedPointer - 1 ])
                    { 
                        //printf("correct bcc2\n");
                        if (duplicate)
                        {     
                            sFrame[0] = FLAG; 
                            sFrame[1] = A_Rx;
                            sFrame[2] = RR(I_number);
                            sFrame[3] = A_Rx ^ RR(I_number);
                            sFrame[4] = FLAG;
                            //printf("sent dup\n");
                            state = START; 
                            pos = 0; 
                            duplicate = FALSE;
                            bcc2 = 0; 
                        }
                        else
                        {
                            I_number = !I_number; // switch I frame
                            sFrame[0] = FLAG; 
                            sFrame[1] = A_Rx;
                            sFrame[2] = RR(I_number);
                            sFrame[3] = A_Rx ^ RR(I_number);
                            sFrame[4] = FLAG;
                            memcpy(packet, destuffedBuffer, destuffedPointer );
                            //printf("sent non dup\n");
                            state = END;
                        }
                        frameRcvSuccessfullyCount++;
                        
                    }
                    else
                    {   
                        //printf("incorrect bcc2\n");
                        if (duplicate)
                        {
                            sFrame[0] = FLAG; 
                            sFrame[1] = A_Rx;
                            sFrame[2] = RR(I_number);
                            sFrame[3] = A_Rx ^ RR(I_number);
                            sFrame[4] = FLAG;
                            //printf("sent dup\n");
                            state = START; 
                            pos = 0; 
                            bcc2 = 0; 
                            

                            
                        }
                        else
                        {
                            sFrame[0] = FLAG; 
                            sFrame[1] = A_Rx;
                            sFrame[2] = REJ(I_number);
                            sFrame[3] = A_Rx ^ REJ(I_number);
                            sFrame[4] = FLAG;
                            //printf("sent non dup\n");
                            //printf("here\n");
                            state = START; 
                            pos = 0; 
                            bcc2 = 0; 
                        }
                        
                    }
                    if (writeBytesSerialPort(sFrame, 5) == -1)
                        return -1;
                    frameSentCount++;
                    //printf("state:%d\n",state);
                    
                }
                break;
            default:
                return -1;
            }
        }
    }
    // cleanup
    free(frame);
    free(destuffedBuffer);
    
    return pos - 1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llcloseTx()
{
    retransmissionCurCount = 0;
    (void)signal(SIGALRM, alarmHandler);

    unsigned char byte;
    while (retransmissionCurCount < retransmissionLimit && state != END)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(timeout);
            alarmEnabled = TRUE;
            unsigned char sFrame[] = {FLAG, A_Tx, DISC, A_Tx ^ DISC, FLAG};
            if (writeBytesSerialPort(sFrame, 5) == -1)
                return -1;
            state = START; 
        }
        int bytes = readByteSerialPort(&byte);
        if (bytes > 0)
        {
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                break;
            case START_FLAG:
                if (byte == A_Rx)
                {
                    state = A;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case A:
                if (byte == DISC)
                {
                    state = C;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case C:
                if (byte == (A_Rx ^ DISC))
                {
                    state = BCC;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC:
                if (byte == FLAG)
                {
                    state = END;
                }
                else
                {
                    state = START;
                }
                break;
            default:
                return -1;
            }
        }
    }

    // cleanup
    alarm(0);
    alarmEnabled = FALSE;
    if (retransmissionLimit <= retransmissionCurCount)
        return -1;
    // send UA
    unsigned char sFrame[] = {FLAG, A_Tx, UA, A_Tx ^ UA, FLAG};
    if (writeBytesSerialPort(sFrame, 5) == -1)
        return -1;
    return 1;
}
int llcloseRx()
{
    unsigned char byte;
    int sndRound = FALSE; // if true, its checking the UA, else its checking the DISC
    while (state != END)
    {
        int bytes = readByteSerialPort(&byte);
        if (bytes > 0)
        {
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = START_FLAG;
                }
                else
                {
                    sndRound = FALSE;
                }
                break;
            case START_FLAG:
                if (byte == A_Tx)
                {
                    state = A;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                    sndRound = FALSE;
                }
                else
                {
                    state = START;
                    sndRound = FALSE;
                }
                break;
            case A:
                if ((byte == DISC && !sndRound) || (byte == UA && sndRound))
                {
                    state = C;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                    sndRound = FALSE;
                }
                else
                {
                    state = START;
                    sndRound = FALSE;
                }
                break;
            case C:
                if ((byte == (A_Tx ^ DISC) && !sndRound) || (byte == (A_Tx ^ UA) && sndRound))
                {
                    state = BCC;
                }
                else if (byte == FLAG)
                {
                    state = START_FLAG;
                    sndRound = FALSE;
                }
                else
                {
                    state = START;
                    sndRound = FALSE;
                }
                break;
            case BCC:
                if (byte == FLAG)
                {
                    if (sndRound)
                    {
                        state = END;
                    }
                    else
                    {
                        state = START;
                        sndRound = TRUE;
                        unsigned char sFrame[] = {FLAG, A_Rx, DISC, A_Rx ^ DISC, FLAG};
                        if (writeBytesSerialPort(sFrame, 5) == -1)
                            return -1;
                    }
                }
                else
                {
                    state = START;
                    sndRound = FALSE;
                }
                break;
            default:
                return -1;
            }
        }
    }
    return 1;
}

int llclose(int showStatistics)
{
    state = START;
    switch (role)
    {
    case LlTx:
        if (llcloseTx() < 0)
            return -1;
        break;
    case LlRx:
        if (llcloseRx() < 0)
            return -1;
    }
    if (showStatistics)
    {
        printf("======================================================\n");
        printf("Total number of frames sent: %d\n", frameSentCount);
        printf("Number of frames retransmissioned: %d\n", retransmissionTotalCount);
        printf("in which %d were retransmissioned due to timeout\n", timeoutCount);
        printf("Total number of frames received successfully: %d\n", frameRcvSuccessfullyCount);
        printf("======================================================\n");
    }
    int clstat = closeSerialPort();
    return clstat;
}
