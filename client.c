
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>

#include "proj.h"

// username to send a private message too
char chatMessageUser[10];


// returns size of username, or -1 if its not found
int findUsername(char* message){
    int i = 1; // start at 1 since 0 is @
    int foundStart = 0;
    int userIndex = 0;
    int size = 0;

    // checks the 13 bytes after the 1st byte which is the @ symbol, if this program finds a word then it returns its size
    // and puts the word character by character into chatMessageUser and returns the size of the username
    while(i < 14){
        if(!isspace(message[i])){
            if(!foundStart){
                foundStart = 1;
            }
            chatMessageUser[userIndex] = message[i];
            userIndex++;
            size++;
        }
        else if(foundStart){
            return size;
        }
        i++;
    }
    return -1;
}


int main(int argc, char **argv) {
    struct hostent *ptrh; /* pointer to a host table entry */
    struct protoent *ptrp; /* pointer to a protocol table entry */
    struct sockaddr_in sad; /* structure to hold an IP address */
    int sd; /* socket descriptor */
    int port; /* protocol port number */
    char *host; /* pointer to host name */

    memset((char *) &sad, 0, sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET; /* set family to Internet */


    if (argc != 4) {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "./client server_address server_port chat_log_file\n");
        exit(EXIT_FAILURE);
    }


    // variable to hold my chat log file
    char* chatFile = argv[3];


    port = atoi(argv[2]); /* convert to binary */
    if (port > 0) /* test for legal value */
        sad.sin_port = htons((u_short) port);
    else {
        fprintf(stderr, "Error: bad port number %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    host = argv[1]; /* if host argument specified */

    /* Convert host name to equivalent IP address and copy to sad. */
    ptrh = gethostbyname(host);
    if (ptrh == NULL) {
        fprintf(stderr, "Error: Invalid host: %s\n", host);
        exit(EXIT_FAILURE);
    }

    memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

    /* Map TCP transport protocol name to protocol number. */
    if (((long int) (ptrp = getprotobyname("tcp"))) == 0) {
        perror("Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /* Create a socket. */
    sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Connect the socket to the specified server. */
    if (connect(sd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    char buf[11]; /* buffer for data from the server */
    char userName[11];
    char tmp_buf[255];
    char chatMessage[257];
    fd_set readyToRead, allSockets; /* Socket sets to use with select */
    int maxFd; /* largest socket number */
    int selectReturn;
    int bytesRead;
    int more;
    int max_ulen;
    int active = 0;
    int userLength = 0;

    uint32_t header32;
    uint32_t networkHeader32;
    uint32_t hostHeader32;
    uint16_t header16;
    uint16_t networkHeader16;
    uint16_t hostHeader16;
    uint8_t mt;
    uint8_t code;
    uint8_t unc;
    uint8_t ulen;

    uint8_t pub, prv, frg, lst;
    uint16_t length;


    // initialize all of my all sockets set to 0, then adding my server socket
    FD_ZERO(&allSockets);
    FD_SET(sd, &allSockets); // read from the server
    FD_SET(0, &allSockets);
    maxFd = sd;
    int timesThrough = 0;

    // recieve the initial acceptance header

    while(1){

        // resetting values
        memset(chatMessageUser, 0, sizeof(chatMessageUser));
        header16 = 0;
        header32 = 0;
        memcpy(&readyToRead, &allSockets, sizeof(fd_set));

        // asking for input like the username. Select will be run if hanging on input 

        // calling select and checking for select error
        if(active){
            PRINT_MSG("Enter message: ");
        }

        if((selectReturn = select(maxFd+1, &readyToRead, NULL, NULL, NULL)) < 0){
            fprintf(stderr,"Select error\n");
            exit(0);
        }

        if(selectReturn){

            // case where im reading from stdin
            if(FD_ISSET(0, &readyToRead)){
                
                if(!active){
                    bytesRead = read_stdin(buf, 11, &more);
                    

                    // username too long case, prompts for the username again
                    if(more){
                        
                        // flushing stdin               
                        while (more) {
                            read_stdin(tmp_buf, 255, &more);
                        }

                        PRINT_wARG("Choose a username (should be less than %d): ", max_ulen);
                    }

                    else{
                       
                        userLength = bytesRead-1;
                        networkHeader16 = 0;
                        create_control_header(&networkHeader16, 0, 4, 0,(uint16_t)userLength);
                        networkHeader16 = htons(networkHeader16);
                        send(sd, &networkHeader16, 2, 0);
                        send(sd, buf, bytesRead-1, 0);
                    }
                }

                // reading message input, active user
                else{
                    uint8_t frag = 0;

                   
                    // reading stdin
                    bytesRead = read_stdin(chatMessage, 256, &more);

                    
                    // private message
                    if(chatMessage[0] == '@'){
                        int usernameLength = findUsername(chatMessage);

                        
                        // false positive
                        if(usernameLength == -1){
                           
                            // do public message protocol 
                            header32 = 0;

                            // creating public message header
                            create_chat_header(&header32, 1, 1, 0, (uint8_t)more, 0, 0, bytesRead-1);
                            networkHeader32 = htonl(header32);
                            send(sd, &networkHeader32, 4, 0);
                            send(sd, chatMessage, bytesRead-1, 0);

                            while(more){
                                frag = 1;
                                header32 = 0;
                                bytesRead = read_stdin(chatMessage, 256, &more);
                                create_chat_header(&header32, 1, 1, 0, frag, (uint8_t)(!more), 0, bytesRead-1);
                                networkHeader32 = htonl(header32);
                                send(sd, &networkHeader32, 4, 0);
                                send(sd, chatMessage, bytesRead-1, 0);
                            }
                        }

                        // private message protocol 
                        else{
                           
                            char prvMessageBuf[255];
                            int prvIndex = 0;
                            int charsInPrv = 0;
                            int afterLastchar = 0;

                            // need to parse original message to send the correct message size which doesnt include the prv user
                         
                            char lastChar = chatMessageUser[usernameLength-1];
                           

                            for(int i = 0; i < bytesRead; i++){
                                if(afterLastchar){
                                    prvMessageBuf[prvIndex] = chatMessage[i];
                                    prvIndex++;
                                    charsInPrv++;
                                }
                                else if(chatMessage[i] == lastChar && isspace(chatMessage[i+1])){
                                    afterLastchar = 1;
                                }
                            }                 

                                 



                            // creating private message header
                            header32 = 0;
                            create_chat_header(&header32, 1, 0, 1, (uint8_t)more, 0, usernameLength, charsInPrv-1);
                            networkHeader32 = htonl(header32);

                            // NEED TO SEND THE CHAT MESSAGE AFTER THE USERNAME
                            send(sd, &networkHeader32, 4, 0);
                            send(sd, chatMessageUser, usernameLength, 0);
                            send(sd, prvMessageBuf, charsInPrv-1, 0);

                            while(more){
                                frag = 1;
                                header32 = 0;
                                bytesRead = read_stdin(chatMessage, 256, &more);
                                create_chat_header(&header32, 1, 0, 1, frag, (uint8_t)(!more), usernameLength, bytesRead-1);
                                networkHeader32 = htonl(header32);
                                send(sd, &networkHeader32, 4, 0);
                                send(sd, chatMessageUser, usernameLength, 0);
                                send(sd, chatMessage, bytesRead-1, 0);
                            }
                            
                        }
                    }

                    // public message
                    else{
                        header32 = 0;
                        create_chat_header(&header32, 1, 1, 0, (uint8_t)more, 0, 0, bytesRead-1);
                        networkHeader32 = htonl(header32);
                        send(sd, &networkHeader32, 4, 0);
                        send(sd, chatMessage, bytesRead-1, 0);

                        while(more){
                            header32 = 0;
                            uint8_t frag = 1;
                            bytesRead = read_stdin(chatMessage, 256, &more);
                            create_chat_header(&header32, 1, 1, 0, frag, (uint8_t)(!more), 0, bytesRead-1);
                            networkHeader32 = htonl(header32);
                            send(sd, &networkHeader32, 4, 0);
                            send(sd, chatMessage, bytesRead-1, 0);
                        }
                    }
                }
            }

            // case where the server sent data
            else{
               
                bytesRead = recv(sd, &mt, 1, MSG_PEEK); // peeking the first byte

                // server closed
                if(bytesRead == 0){
                    PRINT_MSG("Lost connection with the server\n");
                    exit(EXIT_SUCCESS);
                }

                mt = (mt & 0x80) >> 7;

                // control message scenario NEED TO ADD CODE 5 AND CODE 6
                if(mt == 0){
                  
                    recv(sd, &header16, 2, 0);
                   

                    hostHeader16 = ntohs(header16);
                    parse_control_header(&hostHeader16, &mt, &code, &unc, &ulen);
                  

                    
                    if(code == 0){
                        PRINT_MSG("Server is full\n");
                        exit(EXIT_SUCCESS);
                    }

                    if(code == 1){
                        max_ulen = ulen;
                        PRINT_wARG("Choose a username (should be less than %d): ", max_ulen);
                        // call read_stdin here? 
                    }

                    if(code == 2){
                        // bring in the userName
                        bytesRead = recv(sd, userName, ulen, 0);
                        userName[bytesRead] = '\0';
                        FILE* chatLog = fopen(chatFile, "a");
                        fprintf(chatLog, "User %s has joined\n", userName);
                        fflush(chatLog);
                        fclose(chatLog);
                    }

                    if(code == 3){
                        // bring in the userName
                        bytesRead = recv(sd, userName, ulen, 0);
                        userName[bytesRead] = '\0';
                        FILE* chatLog = fopen(chatFile, "a");
                        fprintf(chatLog, "User %s has left\n", userName);
                        fflush(chatLog);
                        fclose(chatLog);
                    }

                    if(code == 4){
                        if(unc == 0){
                            PRINT_MSG("\nTime to enter username has expired. Try again.\n");
                            exit(EXIT_FAILURE);
                        }

                        if(unc == 1){
                            PRINT_MSG("\nUsername is taken. Try again.\n");
                            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen);
                        }

                        if(unc == 2){
                            PRINT_MSG("\nUsername is too long. Try again.\n");
                            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen);
                        }

                        if(unc == 3){
                            PRINT_MSG("\nUsername is invalid. Only alphanumerics are allowed. Try again.\n");
                            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen);
                        }

                        if(unc == 4){
                            PRINT_MSG("\nUsername accepted\n");
                            active = 1;
                            bytesRead = recv(sd, userName, ulen, 0);
                        }
                    }

                    if(code == 5){
                        PRINT_MSG("Couldn't send message. It was too long...\n");
                    }

                    if(code == 6){
                        char* rejectedUsername = (char*)malloc(11);

                        // recieve private username which didnt exist, then add to chat log a warning that it doesnt exist
                        recv(sd, rejectedUsername, ulen, 0);
                        rejectedUsername[ulen] = 0;
                        FILE* chatLog = fopen(chatFile, "a");
                        fprintf(chatLog, "Warning: user %s doesn't exist...\n", rejectedUsername);
                        fflush(chatLog);
                        fclose(chatLog);


                        free(rejectedUsername);

                    }
                }
            
                // case where im recieving a chat message NEED TO DO
                else if(mt == 1){
                    
                    char messageReceived[4096];
                    char nameFrom[11];
                    int nameRcvLength = 0;
                    memset(messageReceived, 0, sizeof(messageReceived));
                    memset(nameFrom, 0, sizeof(messageReceived));
                    

                    // recieving header
                    recv(sd, &header32, 4, 0);
                    hostHeader32 = ntohl(header32);

                    // gettind header values
                    parse_chat_header(&hostHeader32, &mt, &pub, &prv, &frg, &lst, &ulen, &length);

                    // recieve username and message
                    nameRcvLength = recv(sd, nameFrom, ulen, 0);

                    nameFrom[nameRcvLength+1] = '\0'; // adding null byte
                    bytesRead = recv(sd, messageReceived, length, 0);
                    messageReceived[bytesRead+1] = '\0'; // adding null byte

            
                    if(pub){
                        FILE* chatLog = fopen(chatFile, "a");
                        fprintf(chatLog, ">%16s: %s\n", nameFrom, messageReceived);
                        
                        fflush(chatLog);
                        fclose(chatLog);
                    }
                    else if(prv){
                        FILE* chatLog = fopen(chatFile, "a");
                        fprintf(chatLog, "%%%16s: %s\n", nameFrom, messageReceived);
                        fflush(chatLog);
                        fclose(chatLog);
                    }

                    
                }

                // error case
                else{
                    printf("Received invalid mt value...\n");
                    exit(0);
                }
            }

            timesThrough++;
        }
    }

    exit(EXIT_SUCCESS);
}
