/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <regex.h>

#include "download.h"

int parseURL(char* url, URL* urlParsed) {
    const char* pattern = "^ftp://(([^:@/]+)(:([^@/]+))?@)?([^/]+)(/([^/]+/)*([^/]+))$";
    regex_t regex;
    regmatch_t matches[9];  

    // Compile the regex
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Failed to compile regex.\n");
        return 1;
    }

    // Execute regex
    if (regexec(&regex, url, 9, matches, 0) == 0) {
        printf("Match found!\n");

        // Username
        if (matches[2].rm_so != -1) {
            int len = matches[2].rm_eo - matches[2].rm_so;
            urlParsed->username = malloc(len + 1);
            snprintf(urlParsed->username, len + 1, "%.*s", len, url + matches[2].rm_so);
            printf("Username: %s\n", urlParsed->username);
        } else {
            printf("No username found. Defaulting to 'anonymous'.\n");
            urlParsed->username = strdup("anonymous");
        }

        // Password
        if (matches[4].rm_so != -1) {
            int len = matches[4].rm_eo - matches[4].rm_so;
            urlParsed->password = malloc(len + 1);
            snprintf(urlParsed->password, len + 1, "%.*s", len, url + matches[4].rm_so);
            printf("Password: %s\n", urlParsed->password);
        } else {
            printf("No password found. Defaulting to 'anonymous'.\n");
            urlParsed->password = strdup("anonymous");
        }

        // Hostname
        if (matches[5].rm_so != -1) {
            int len = matches[5].rm_eo - matches[5].rm_so;
            urlParsed->hostName = malloc(len + 1);
            snprintf(urlParsed->hostName, len + 1, "%.*s", len, url + matches[5].rm_so);
            printf("Hostname: %s\n", urlParsed->hostName);
        } else {
            printf("No hostname found.\n");
            return 1;
        }

        // Path (excluding file name)
        if (matches[6].rm_so != -1) {
            int len = matches[6].rm_eo - matches[6].rm_so;
            urlParsed->path = malloc(len + 1);
            snprintf(urlParsed->path, len + 1, "%.*s", len, url + matches[6].rm_so);
            printf("Path: %s\n", urlParsed->path);
        }

        // File name
        if (matches[8].rm_so != -1) {
            int len = matches[8].rm_eo - matches[8].rm_so;
            urlParsed->fileName = malloc(len + 1);  
            snprintf(urlParsed->fileName, len + 1, "%.*s", len, url + matches[8].rm_so);
            printf("File Name: %s\n", urlParsed->fileName);
        } else {
            printf("No file name found.\n");
            return 1;
        }
    } else {
        printf("No match found.\n");
        return 1;
    }

    // Free the regex resources
    regfree(&regex);
    return 0;
}


int receiveResponse(int sockfd, Response* response){
    int start_line_pointer = 0;
    int total_length = 0;
    while(1){
        int bytes_received = read(sockfd, response->message + total_length, 1);
        if (bytes_received > 0) {  
            total_length += bytes_received;
            if(total_length >= 2 &&  response->message[total_length - 2] == '\r' && response->message[total_length - 1] == '\n'){
                if(response->message[start_line_pointer + 3] != '-' && response->message[start_line_pointer] >= '0' && response->message[start_line_pointer] <= '9' && response->message[start_line_pointer + 1] >= '0' && response->message[start_line_pointer + 1] <= '9' && response->message[start_line_pointer + 2] >= '0' && response->message[start_line_pointer + 2] <= '9'){
                    response->message[total_length] = '\0';
                    char code[4];
                    memcpy(code, response->message, 3);
                    code[3] = '\0';
                    response->code = atoi(code);
                    break;
                }
                else{
                    start_line_pointer = total_length;
                    continue;
                }
            }
        }
    }
    
    return 0; 
}

int sendString( int sockfd, char* message){
    int bytesSent = write(sockfd, message, strlen(message)) ;
    if( bytesSent <= 0){
        printf("No bytes sent\n");
        return 1;
    }
    return 0; 
}
int receiveWelcomeMessage(int sockfd, Response* response){
    if(receiveResponse(sockfd,response)) return 1; 
    
    printf(response->message);

    if(response->code != 220) return 1; 
    
    
    
    return 0; 
}
int sendUser(int sockfd, char* username, Response* response){
    char message[MSG_SIZE];
    sprintf(message, "USER %s\r\n", username);
    printf(message);
    
    if(sendString(sockfd, message)) return 1; 
    if(receiveResponse(sockfd,response)) return 1; 
    printf(response->message);
    if(response->code != 331) return 1; 
    
    return 0; 
}

int sendPassword(int sockfd, char* password, Response* response){
    char message[MSG_SIZE];
    sprintf(message, "PASS %s\r\n", password);
    printf(message);
    
    if(sendString(sockfd, message)) return 1; 
    
    if(receiveResponse(sockfd,response)) return 1; 
    
    printf(response->message);
    if(response->code != 230) return 1; 
    
    return 0; 
}
int sendType(int sockfd, Response* response){
    char message[MSG_SIZE];
    sprintf(message, "TYPE I\r\n");
    printf(message);
    
    if(sendString(sockfd, message)) return 1; 
    
    if(receiveResponse(sockfd,response)) return 1; 
    
    printf(response->message);
    if(response->code != 200) return 1; 
    
    return 0; 
}
int getFileSize(int sockfd, char* path, int* file_size){
    char message[MSG_SIZE];
    sprintf(message, "SIZE %s\r\n", path);
    printf(message);
    
    Response response;
    if(sendString(sockfd, message)) return 1; 
    
    if(receiveResponse(sockfd,&response)) return 1; 
    
    printf(response.message);
    if(response.code != 213) return 1; 
    
    sscanf(response.message, "213 %d", file_size);
    
    return 0; 
}

int enterPassiveMode(int sockfd_command, int* sockfd_file){
    char message[MSG_SIZE];
    sprintf(message, "PASV\r\n");
    printf(message);

    Response response;
    if(sendString(sockfd_command, message)) return 1;

    if(receiveResponse(sockfd_command,&response)) return 1;

    printf(response.message);
    if(response.code != 227) return 1;

    int ip1, ip2, ip3, ip4, port1, port2;
    sscanf(response.message, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2);

    char ip[16];
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    struct sockaddr_in server_addr;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/    
    server_addr.sin_port = htons(port1 * 256 + port2);        
    /*open a TCP socket*/
    if ((*sockfd_file = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to open socket\n");
        return 1;
    }
    /*connect to the server*/
    if (connect(*sockfd_file,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect()");
        printf("Unable to connect to server\n");
        return 1; 
    }


    return 0; 
}

int getFile(int sockfd_command, int sockfd_file, char* path, char* fileName, int file_size){
    char message[MSG_SIZE];
    sprintf(message, "RETR %s\r\n", path);
    printf(message);

    Response response;
    if(sendString(sockfd_command, message)) return 1;

    if(receiveResponse(sockfd_command,&response)) return 1;

    printf(response.message);
    if(response.code < 100 && response.code >= 200) return 1;

    FILE* file = fopen(fileName, "wb");
    if(file == NULL){
        printf("Failed to open file\n");
        return 1; 
    }
    int total_bytes_received = 0;
    while(total_bytes_received < file_size){
        char buffer[MSG_SIZE];
        int bytes_received = read(sockfd_file, buffer, MSG_SIZE);
        if(bytes_received <= 0){
            printf("Failed to read from socket\n");
            return 1; 
        }
        total_bytes_received += bytes_received;
        fwrite(buffer, 1, bytes_received, file);
    }
    fclose(file);

    if(receiveResponse(sockfd_command,&response)) return 1;
    
    printf(response.message);
    if(response.code < 200) return 1;

    return 0; 
}

int quit(int sockfd, Response* response){
    char message[MSG_SIZE];
    sprintf(message, "QUIT\r\n");
    printf(message);
    
    if(sendString(sockfd, message)) return 1; 
    
    if(receiveResponse(sockfd,response)) return 1; 
    
    printf(response->message);
    if(response->code != 221) return 1; 
    
    return 0; 
}

int main(int argc, char **argv) {
    if(argc != 2){
        printf("Need to provide a server ip to connect to\n");
        return 1; 
    }
    URL url; 

    if(parseURL(argv[1], &url)){
        printf("Failed to parse URL\n");
        return 1;
    }  
    
    struct hostent *command_host;
    if ((command_host = gethostbyname(url.hostName)) == NULL) {
        printf("Could not get host by name\n");
        return 1;
    }

    printf("Host name  : %s\n", command_host->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) command_host->h_addr)));

    int sockfd_command;
    struct sockaddr_in server_addr;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) command_host->h_addr)));    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(21);        
    /*open a TCP socket*/
    if ((sockfd_command = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to open socket\n");
        return 1;
    }
    /*connect to the server*/
    if (connect(sockfd_command,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect()");
        printf("Unable to connect to server\n");
        return 1; 
    }
    Response response;
    response.code = 0;
    if(receiveWelcomeMessage(sockfd_command,&response)){
        printf("Failed to receive welcome message\n");
        return 1; 
    }
    if(sendUser(sockfd_command, url.username, &response)){
        printf("Failed to send user\n");
        return 1; 
    }
    if(sendPassword(sockfd_command, url.password, &response)){
        printf("Failed to send password\n");
        return 1; 
    }
    if(sendType(sockfd_command, &response)){
        printf("Failed to send type\n");
        return 1; 
    }
    int file_size = 0;
    if(getFileSize(sockfd_command, url.path, &file_size)){
        printf("Failed to get file size\n");
        return 1; 
    }
    int sockfd_file;


    if(enterPassiveMode(sockfd_command, &sockfd_file)){
        printf("Failed to enter passive mode\n");
        return 1; 
    }
    
    if(getFile(sockfd_command, sockfd_file, url.path, url.fileName, file_size)){
        printf("Failed to get file\n");
        return 1; 
    }

    if(quit(sockfd_command, &response)){
        printf("Failed to quit\n");
        return 1; 
    }

    if (close(sockfd_command)<0) {
        printf("Failed to close socket\n");
        return 1; 
    }
    
    if(close(sockfd_file)<0){
        printf("Failed to close socket\n");
        return 1; 
    }


    return 0; 
}


