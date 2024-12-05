#define MSG_SIZE 1024
typedef struct { 
    char* username; 
    char* password; 
    char* hostName; 
    char* path; 
} URL; /* Struct to store the info parsed by parseURL*/


typedef struct{
    int code; 
    char message[MSG_SIZE];
} Response; /* Struct to store the response from the server*/