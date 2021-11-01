

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

// Client info struct
    typedef struct clientInfo{
        int socketNum;
        int status; // 0 for inactive, 1 for user negotiation, 2 for active
        char userName[11];
        time_t timeEntered;
        time_t kickTime; 
        int maxUserLength;
        int maxPossibleClients;
        char chatMessage[4095];
        int chatMessageSize;
    } clientInfo;



#define QLEN 2 /* size of request queue */
int visits = 0; /* counts client connections */
clientInfo* allClients; /* All my clients info*/

/*
    This method takes in a list of clients, and then checks if the client is active, then checks if there username is the same as the one attempting to be made
    Returns a 1 for true if the username is found, and a 0 for false it wasnt found meaning its available. 
    */
    int userNameInUse( char* userToTest){
     
        for(int i = 0; i < 255; i++){
            if(allClients[i].status != 0){
                if(strcmp(allClients[i].userName, userToTest) == 0){
                    return 1;
                }
            }
        }
        return 0;
    }

    // checks if a user name uses valid characters returns a int for true/false
    int validUserChars(char* userToTest, int nameSize){

        // checks if each char falls in the an invalid range of characters
        for(int i = 0; i < nameSize; i++){
            if(
                (userToTest[i] < 48 || userToTest[i] > 122) ||
                (userToTest[i] > 57 && userToTest[i] < 65) ||
                (userToTest[i] > 90 && userToTest[i] < 97))
                {
                    return 0;
                }
        }
        // no chars invalid so the user is valid
        return 1;
    }


// takes in the server socket, and a sockaddr_in 
    int acceptConnection(int server, struct sockaddr_in sockAddr){
        int newSocket;
        size_t size = sizeof(struct sockaddr_in);

        newSocket = accept(server, (struct sockaddr*) &sockAddr, &size);
        
        if(newSocket < 0){
            printf("Error: Accept failed\n");
            return -1;
        }
        else{
            return newSocket;
        }
    }

    /*
    This method takes in a header, the message recieved, the socket it came from, the length, the index in my clients array, and the user name for a private msg

    The method processes a chat message based on the recieved header and either sends messages out or saves the fragment.
    */
    void handleChatMessage(uint32_t header, char* message, int socket, int messageLength, int clientIndex, char* prvMsgRecipient){

        // vars for header
        uint8_t mt;
        uint8_t pub;
        uint8_t prv;
        uint8_t frg;
        uint8_t lst;
        uint8_t ulen;
        uint16_t length;
        uint32_t networkHeader32;
        uint16_t networkHeader16;

        parse_chat_header(&header, &mt, &pub, &prv, &frg, &lst, &ulen, &length);

        
        // public message case
        if(pub){

            // frg case
            if(frg){

                // not last case
                if(!lst){

                    // going through the whole message
                    for(int i = 0; i < messageLength; i++){

                        // checking if message is too large
                        if(allClients[clientIndex].chatMessageSize == 4095){

                            // send to this client that there message is too long
                            create_control_header(&networkHeader16, 0, 5, 0, 0);
                            networkHeader16 = htons(networkHeader16);
                            send(socket, &networkHeader16, 2, 0);

                            // reset my chatMessageSize and my chat message array
                            allClients[clientIndex].chatMessageSize = 0;
                            memset(allClients[clientIndex].chatMessage, 0, 4095);
                            return;

                        }
                        
                        // add character by character to my message buffer
                        allClients[clientIndex].chatMessage[allClients[clientIndex].chatMessageSize] = message[i];
                        allClients[clientIndex].chatMessageSize++;
                        
                    }
                }
                // last frag case of public message
                if(lst){
                    for(int i = 0; i < messageLength; i++){
                        if(allClients[clientIndex].chatMessageSize == 4095){

                            // send to this client that there message is too long
                            create_control_header(&networkHeader16, 0, 5, 0, 0);
                            networkHeader16 = htons(networkHeader16);
                            send(socket, &networkHeader16, 2, 0);

                            // reset my chatMessageSize and chatMessage
                            allClients[clientIndex].chatMessageSize = 0;
                            memset(allClients[clientIndex].chatMessage, 0, 4095);
                            return;
                        }
                        allClients[clientIndex].chatMessage[allClients[clientIndex].chatMessageSize] = message[i];
                        allClients[clientIndex].chatMessageSize++;
                    }

                    // send message to everyone
                    for(int i = 0; i < 255; i++){
                        if(allClients[i].status == 2){
                            networkHeader32 = 0;
                            create_chat_header(&networkHeader32, 1, 1, 0, 0, 0, strlen(allClients[clientIndex].userName),allClients[clientIndex].chatMessageSize);
                            networkHeader32 = htonl(networkHeader32);
                          
                            send(allClients[i].socketNum, &networkHeader32, 4, 0);
                            send(allClients[i].socketNum, allClients[clientIndex].userName,strlen(allClients[clientIndex].userName), 0);
                            send(allClients[i].socketNum, allClients[clientIndex].chatMessage, allClients[clientIndex].chatMessageSize, 0);
                        }
                    }
                    // reset my chatMessageSize and chatMessage
                    allClients[clientIndex].chatMessageSize = 0;
                    memset(allClients[clientIndex].chatMessage, 0, 4095);
                    return;  
                }
            }

            // public message which isnt a fragment 
            else{

                // send message to everyone
                for(int i = 0; i < 255; i++){
                    if(allClients[i].status == 2){
                        networkHeader32 = 0;
                        create_chat_header(&networkHeader32, 1, 1, 0, 0, 0, strlen(allClients[clientIndex].userName),messageLength);
                        networkHeader32 = htonl(networkHeader32);
                     
                        send(allClients[i].socketNum, &networkHeader32, 4, 0);
                        send(allClients[i].socketNum, allClients[clientIndex].userName,strlen(allClients[clientIndex].userName), 0);
                        send(allClients[i].socketNum, message, messageLength, 0);
                    }
                }
            }
        }

        // private message
        else if(prv){

            // vars to track private message info
            int validUser = 0;
            int recipientSocket;
            

           
            
            // check if user exists, if it does save the recipient socket and index
            for(int i = 0; i < 255; i++){
                if(strncmp(allClients[i].userName, prvMsgRecipient, ulen) == 0){
                    validUser = 1;
                    recipientSocket = allClients[i].socketNum;
                }
            }

           // if not a valid user then let the sender know
            if(!validUser){
                // send to client that the user doesnt exist
                networkHeader16 = 0;
                create_control_header(&networkHeader16, 0, 6, 0, ulen);
                networkHeader16 = htons(networkHeader16);
                send(socket, &networkHeader16, 2, 0);
                send(socket, prvMsgRecipient, ulen, 0);

                allClients[clientIndex].chatMessageSize = 0;
                return;
            }
            

            // frag of a prv msg case
            if(frg){
                
                // not last frag case
                if(!lst){
                    
                    // adding message to my message buffer
                    for(int i = 0; i < messageLength; i++){

                        // checking if message to large
                        if(allClients[clientIndex].chatMessageSize == 4095){

                            // send to this client that there message is too long
                            create_control_header(&networkHeader16, 0, 5, 0, 0);
                            networkHeader16 = htons(networkHeader16);
                            send(socket, &networkHeader16, 2, 0);
                            
                            // reset the message size and message and return
                            allClients[clientIndex].chatMessageSize = 0;
                            memset(allClients[clientIndex].chatMessage, 0, 4095);
                            return;
                        }
                        allClients[clientIndex].chatMessage[allClients[clientIndex].chatMessageSize] = message[i];
                        allClients[clientIndex].chatMessageSize++;
                    }
                }

                // last frag case
                if(lst){
                    for(int i = 0; i < messageLength; i++){
                        
                        // message too large
                        if(allClients[clientIndex].chatMessageSize == 4095){

                            // send to this client that there message is too long
                            create_control_header(&networkHeader16, 0, 5, 0, 0);
                            networkHeader16 = htons(networkHeader16);
                            send(socket, &networkHeader16, 2, 0);

                            // reset the message size and return
                            allClients[clientIndex].chatMessageSize = 0;
                            memset(allClients[clientIndex].chatMessage, 0, 4095);
                            return;
                        }
                        allClients[clientIndex].chatMessage[allClients[clientIndex].chatMessageSize] = message[i];
                        allClients[clientIndex].chatMessageSize++;
                    }

                    // send message to specific user
                    networkHeader32 = 0;
                    // header to send to the recipient
                    create_chat_header(&networkHeader32, 1, 0, 1, 0, 0, strlen(allClients[clientIndex].userName), allClients[clientIndex].chatMessageSize);
                    networkHeader32 = htonl(networkHeader32);

                    // sending the header, username, and the message to the recipient
                    send(recipientSocket, &networkHeader32, 4, 0);
                    send(recipientSocket, allClients[clientIndex].userName, strlen(allClients[clientIndex].userName), 0);
                    send(recipientSocket, allClients[clientIndex].chatMessage, allClients[clientIndex].chatMessageSize, 0);

                    // creating the header to send back to the sender for confirmation
                    networkHeader32 = 0;
                    create_chat_header(&networkHeader32, 1, 0, 1, 0, 0, ulen, allClients[clientIndex].chatMessageSize);
                    networkHeader32 = htonl(networkHeader32);

                    // sending the header, username, and the message
                    send(socket, &networkHeader32, 4, 0);
                    send(socket, prvMsgRecipient, ulen, 0);
                    send(socket, allClients[clientIndex].chatMessage, allClients[clientIndex].chatMessageSize, 0);
                    
                    // reset the message size and return
                    allClients[clientIndex].chatMessageSize = 0;
                    memset(allClients[clientIndex].chatMessage, 0, 4095);
                    return;

                }
            }

            // private message which isnt a fragment 
            else{
                // send message to specific user
                // send message to specific user
                networkHeader32 = 0;
                // header to send to the recipient
                create_chat_header(&networkHeader32, 1, 0, 1, 0, 0, strlen(allClients[clientIndex].userName), messageLength);
                networkHeader32 = htonl(networkHeader32);

                // sending the header, username, and the message
                send(recipientSocket, &networkHeader32, 4, 0);
                send(recipientSocket, allClients[clientIndex].userName, strlen(allClients[clientIndex].userName), 0);
                send(recipientSocket, message, messageLength, 0);

                // creating the header to send back to the sender for confirmation
                networkHeader32 = 0;
                create_chat_header(&networkHeader32, 1, 0, 1, 0, 0, ulen, messageLength);
                networkHeader32 = htonl(networkHeader32);

                // sending the header, username, and the message
                send(socket, &networkHeader32, 4, 0);
                send(socket, prvMsgRecipient, ulen, 0);
                send(socket, message, messageLength, 0);

                // reset the message size and return
                allClients[clientIndex].chatMessageSize = 0;
                memset(allClients[clientIndex].chatMessage, 0, 4095);
                return;
            }

        }

        else{
            printf("Error, the message is neither public or private...\n");
        }


    }
    

    // method to handle all possible control messages
    void handleControlMessage(uint16_t header, char* message, int socket, int nameSize){
        uint8_t mt;
        uint8_t code;
        uint8_t unc;
        uint8_t ulen;
        uint16_t hostHeader = 0;
        uint16_t networkHeader = 0;
        message[nameSize+1] = '\0';

        hostHeader = ntohs(header);
        parse_control_header(&hostHeader, &mt, &code, &unc, &ulen);
         
        if(code == 4){
            
            // username taken 
           
            if(userNameInUse(message)){
              
                networkHeader = 0;
                create_control_header(&networkHeader, 0, 4, 1, 0);
                networkHeader = htons(networkHeader);
                send(socket, &networkHeader, 2, 0);
            }

            else if(nameSize > allClients[0].maxUserLength){
            
                networkHeader = 0;
                create_control_header(&networkHeader, 0, 4, 2, 0);
                networkHeader = htons(networkHeader);
                send(socket, &networkHeader, 2, 0);
            }

            else if(!validUserChars(message, nameSize)){
             
                networkHeader = 0;
                create_control_header(&networkHeader, 0, 4, 3, 0);
                networkHeader = htons(networkHeader);
                send(socket, &networkHeader, 2, 0);
            }

            // the user isnt taken and is valid, because the network header was never changed meaning I never sent a message to client 
            if(networkHeader == 0){
                
                for(int j = 0; j < 255; j++){
                    // sending to newly added client that they have been accepted
                    if(allClients[j].socketNum == socket){
                      
                        strcpy(allClients[j].userName, message);
                        allClients[j].status = 2;

                        networkHeader = 0;
                        create_control_header(&networkHeader, 0, 4, 4, ulen);
                        networkHeader = htons(networkHeader);
                      
                        send(socket, &networkHeader, 2, 0);
                        send(socket, message, nameSize, 0);
                    }

                    // sending to all active clients that a new user joined and there username
                    if(allClients[j].status == 2){
                        networkHeader = 0;
                        create_control_header(&networkHeader, 0, 2, 0, ulen);
                        networkHeader = htons(networkHeader);
                      
                        send(allClients[j].socketNum, &networkHeader, 2, 0);
                        send(allClients[j].socketNum, message, nameSize, 0);
                    }
                }
            }
        }
    }

    
    // Reads the socket data into a char* buffer and returns the amount of bytes read
    int readFromClient(int socket, char* buffer, uint16_t* header, int clientIndex){
        int bytesRead = 0;

        // vars for 16bit header
        uint8_t mt;
        uint8_t code;
        uint8_t unc;
        uint8_t ulen;
        uint16_t hostHeader16;
        uint16_t header16;

        // vars for 32bit header
        uint8_t pub;
        uint8_t prv;
        uint8_t frg;
        uint8_t lst;
        uint16_t length;
        uint32_t header32;
        uint32_t hostHeader32;

        // checking if its a chat or control message
        bytesRead = recv(socket, &mt, 1, MSG_PEEK); // peeking the first byte
        mt = (mt & 0x80) >> 7;

        // handling a control message
        if(mt == 0){

            // need to decode the header
            bytesRead = recv(socket, &header16, 2, 0);
            
            // convert and create header
            hostHeader16 = ntohs(header16);
            parse_control_header(&hostHeader16, &mt, &code, &unc, &ulen);


            // client disconnected
            if(bytesRead == 0){
                return 0;
            }

            // grab message
            if(ulen != 0){
                bytesRead = recv(socket, buffer, ulen, 0); 

                if(bytesRead < 0 ){
                    fprintf(stderr, "Error reading from socket\n");
                    return -1;
                }
                handleControlMessage(header16, buffer, socket, ulen);
            }
        }

        // handling a chat message
        else if(mt == 1){
            // array to hold my prv msg username
            char* prvMsgRecipient = (char*)malloc(11);

            // receive header
            bytesRead = recv(socket, &header32, 4, 0);
            
            hostHeader32 = ntohl(header32);
            parse_chat_header(&hostHeader32, &mt, &pub, &prv, &frg, &lst, &ulen, &length);

            // client disconnected
            if(bytesRead == 0){
              
                return 0;
            }

            // grab message and username
            if(length != 0){

                // grab the username if its a private message
                if(prv){
                    recv(socket, prvMsgRecipient, ulen, 0);
                }

                // recieve the message
                bytesRead = recv(socket, buffer, length, 0); 

                if(bytesRead < 0 ){
                    fprintf(stderr, "Error reading from socket\n");
                    return -1;
                }

                // handle the chat message
                handleChatMessage(hostHeader32, buffer, socket, length, clientIndex, prvMsgRecipient);  
            }

            return bytesRead;
            free(prvMsgRecipient);
        }

        else{
            printf("Invalid mt from client...\n");
        }

        return bytesRead;

    }


int main(int argc, char **argv) {
    struct protoent *ptrp; /* pointer to a protocol table entry */
    struct sockaddr_in sad; /* structure to hold server's address */
    struct sockaddr_in cad; /* structure to hold client's address */
    int sd, sd2; /* socket descriptors */
    int port; /* protocol port number */
    int optval = 1; /* boolean value when we set socket option */
    int maxClients;
    int timeout;
    int maxUlen;

    // checking num of args
    if (argc != 5) {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "./server server_port max_clients timeout max_ulen\n");
        exit(EXIT_FAILURE);
    }

    // setting up input arguments to be used
    maxClients = atoi(argv[2]);
    timeout = atoi(argv[3]);
    maxUlen = atoi(argv[4]);
    
    
    // checking valididty of inputs
    if(maxClients < 1 || maxClients > 255){
        fprintf(stderr, "Error: Invalid number of clients\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "0 < max_clients < 256\n");
        exit(EXIT_FAILURE);
    }

    if(timeout < 1 || timeout > 299){
        fprintf(stderr, "Error: Invalid timeout length\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "0 < timeout < 300\n");
        exit(EXIT_FAILURE);
    }

    if(maxUlen < 1 || maxUlen > 10){
        fprintf(stderr, "Error: Invalid max username length\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "0 < max_ulen < 11\n");
        exit(EXIT_FAILURE);
    }

    

    memset((char *) &sad, 0, sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET; /* set family to Internet */
    sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */


    port = atoi(argv[1]); /* convert argument to binary */
    if (port > 0) { /* test for illegal value */
        sad.sin_port = htons((u_short) port);
    } else { /* print error message and exit */
        fprintf(stderr, "Error: Bad port number %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    /* Map TCP transport protocol name to protocol number */
    if (((long int) (ptrp = getprotobyname("tcp"))) == 0) {
        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /* Create a socket */
    sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    /* Allow reuse of port - avoid "Bind failed" issues */
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Error setting socket option");
        exit(EXIT_FAILURE);
    }

    /* Bind a local address to the socket */
    if (bind(sd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }


    /* Specify size of request queue */
    if (listen(sd, QLEN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }


    int bytesIn; /* Bytes read in */
    char* buf; /* buffer for string the server sends */
    int totalClients = 0; /* Total clients connected currently */
    fd_set readyToRead, allSockets; /* Socket sets to use with select */
    int maxFd; /* largest socket number */
    int selectReturn;
    struct timeval tv;
    allClients = (clientInfo*)malloc(255 * sizeof(clientInfo));

    //initialize all clients to status of inactive, and holding max user length and maxClients. 
    for(int i = 0; i < 255; i++){
        allClients[i].socketNum = -1;
        allClients[i].status = 0;
        allClients[i].maxUserLength = maxUlen; 
        allClients[i].maxPossibleClients = maxClients;
        allClients[i].chatMessageSize = 0;
    }

    // header variables
    uint16_t header;
    uint16_t networkHeader;
    uint8_t mt;
    uint8_t code;
    uint8_t unc;
    uint8_t ulen;

    // initialize all of my all sockets set to 0, then adding my server socket
    FD_ZERO(&allSockets);
    FD_SET(sd, &allSockets);
    maxFd = sd;

    /* Main server loop - accept and handle requests */
    while (1) {

        // resetting my time values and my fd sets
        tv.tv_sec = 1; 
        tv.tv_usec = 0;
        header = 0; // reset header
        memcpy(&readyToRead, &allSockets, sizeof(fd_set));
        
        // Calling select on a 1 second timer, and checking for error 
        if((selectReturn = select(maxFd+1, &readyToRead, NULL, NULL, &tv)) < 0){
            fprintf(stderr,"Select error\n");
            free(allClients);
            exit(0);
        }

        // select has stuff to read
        if(selectReturn){
        // check all sockets
            for(int i = 0; i < maxFd+1; i++){
                if(FD_ISSET(i, &readyToRead)){

                    // reading from server socket
                    if(i == sd){
                        
                        // accept new user case
                        if(totalClients < maxClients){
                            int newSocket = acceptConnection(sd, cad);

                            if(newSocket > 0){
                                FD_SET(newSocket, &allSockets);
                                totalClients++;

                                // update max socket
                                if(newSocket > maxFd) {
                                    maxFd = newSocket;
                                }

                                // find a spot in my client array which is free
                                for(int j = 0; j < 255; j++){
                                    if(allClients[j].status == 0){
                                        allClients[j].status = 1;
                                        allClients[j].socketNum = newSocket;
                                        allClients[j].timeEntered = time(NULL);
                                        allClients[j].kickTime = allClients[j].timeEntered + timeout;
                                        break;                    
                                    }
                                }

                                // send the message that the client can start user negotiation
                                create_control_header(&header, mt = 0, code = 1, unc = 0, ulen = 10);
                                networkHeader = htons(header);
                                send(newSocket, &networkHeader, 2, 0);
                            }
                            // else case, connection error
                            else{
                                fprintf(stderr, "Connection error for adding new socket\n");
                            }
                        }
                        // else send back to this person server is full.
                        else{
                            create_control_header(&header, mt = 0, code = 0, unc = 0, ulen = 0);
                            networkHeader = htons(header);
                            send(i, &networkHeader, 2, 0); // Sending control message saying that the server is full.
                        }
                    }

                    // reading from a client
                    else if(i != sd){

                       
                       
                        // finding the index of the client in my array of structs
                        int clientIndex;
                        for(int j = 0; j < 255; j++){
                            if(allClients[j].socketNum == i){
                                clientIndex = j;
                            }
                        }

                        // mallocing and freeing my buf and reading from client
                        buf = (char*)malloc(257);
                        bytesIn = readFromClient(i, buf, &header, clientIndex);
                        free(buf);

                        // case where the client closes 
                        if(bytesIn == 0){
                            char* leftUser = (char*)malloc(11);
                            int hasUserFlag = 0;
                            FD_CLR(i, &allSockets);
                            // update this client infos status to 0.
                            for(int j = 0; j < 255; j++){
                                if(allClients[j].socketNum == i){
                                    if(allClients[j].status == 2){
                                        memcpy(leftUser, allClients[j].userName, strlen(allClients[j].userName));
                                       
                                        hasUserFlag = 1;
                                    }
                                    allClients[j].status = 0;
                                    allClients[j].socketNum = -1;
                                    memset(allClients[j].chatMessage, 0, 4095);
                                    memset(allClients[j].userName, 0, 11);
                                }
                            }

                            // send to all other sockets that a client has left
                            if(hasUserFlag){
                             
                                for(int j = 0; j < 255; j++){
                                    if(allClients[j].status == 2){
                                        header = 0;
                                        create_control_header(&header, 0, 3, 0, strlen(leftUser)-1);
                                        networkHeader = htons(header);
                                        send(allClients[j].socketNum, &networkHeader, 2, 0);
                                        send(allClients[j].socketNum, leftUser, strlen(leftUser)-1, 0);
                                    }
                                }
                            }
                            free(leftUser);
                            totalClients--;
                            
                            close(i); // close connection
                            
                        }
                    }
                }
            }
        }

        // timeout case
        else{
            // looking for timeouts which occurred
            for(int j = 0; j < 255; j++){
                // checking if they are in the user negotiation phase
                if(allClients[j].status == 1){
                    time_t currTime = time(NULL);
                    if(allClients[j].kickTime < currTime){

                        
                        // send to this client that they took too long then disconnect and make that index available in my array
                        create_control_header(&header, 0, 4, 0, 0);
                        networkHeader = htons(header);
                        
                        send(allClients[j].socketNum, &networkHeader, 2, 0);

                        close(allClients[j].socketNum);

                        // resetting index values
                        memset(allClients[j].userName, 0, 11);
                        memset(allClients[j].chatMessage, 0, 4095);
                        allClients[j].status = 0;
                        allClients[j].socketNum = -1;
                        totalClients--;
                    }
                }
            }
        }
    }

    free(allClients);
    close(sd2);
    
}
