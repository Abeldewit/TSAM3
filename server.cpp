//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is) && Abel de Wit, Hartmann Ingvarsson
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <thread>
#include "ip.cpp"

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections


// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
public:
    int sock;                                           // socket of client connection
    std::string GROUP_ID = "NoName";                    // Group ID of the server
    std::string HOST_IP = "NoIP";                       // IP address of client*/
    std::string SERVPORT = "NoPort";                    // Port of the server

    std::chrono::system_clock::time_point timeout =
            std::chrono::system_clock::now();           // Counter for KEEPALIVE
    int attempts = 0;                                   // Counter for login attempts

    Client(int socket) : sock(socket){}

    ~Client(){}            // Virtual destructor defined for base class
};

/* Definitions */
std::string serverID = "P3_GROUP_100";
std::map<int, Client*> clients; // Lookup table for per Client information
std::map<std::string, std::vector<std::string>> msgLog;
int myPort;
std::string myAddress;
int mainClient = -1;

int open_socket(int portno)
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

    // Create socket for connection. Set to be non-blocking, so recv will
    // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket\n");
        return(-1);
    }
#else
    if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }

#ifdef __APPLE__
    if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SOCK_NOBBLOCK");
    }
#endif
    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family      = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons(portno);

    // Bind to socket to listen for connections from clients

    if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return(-1);
    }
    else
    {
        return(sock);
    }
}

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
    // Remove client from the clients list
    clients.erase(clientSocket);
    // If it is our own client and not a server then we remove that from memory
    if (clientSocket == mainClient) {
        mainClient = 0;
    }


    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if(*maxfds == clientSocket)
    {
        for(auto const& p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.
    FD_CLR(clientSocket, openSockets);
}

/* Loggin with timestamps */
void logOut(std::string log) {
    time_t current_time = time(NULL);
    std::cout << ctime(&current_time);
    std::cout << log << std::endl;
}

/* Sending across the network with SOI and EOI */
int sendCommand(int clientSocket, std::string msg) {
    if(msg[msg.length()-1] == 0x0a) {
        msg.pop_back();
    }
    std::cout << "Sending: " << msg << std::endl;
    int n = msg.length();
    char buffer[n+2];
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, msg.c_str());
    memmove(buffer + 1, buffer, sizeof(buffer));
    buffer[0] = 1;
    buffer[n+1] = 4;

    if(send(clientSocket, buffer, sizeof(buffer), 0) > 0) {
        clients[clientSocket]->timeout = std::chrono::system_clock::now();
        return 0;
    } else {
        return 1;
    }

}

/* Commands the client can issue */
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, std::vector<std::string> tokens) {
    if( tokens[0].compare("SENDMSG") == 0) {
        std::string msg;
        for(int i = 2; i < tokens.size(); i++) {
            msg += tokens[i];
            if(i != tokens.size()-1) {
                msg += ",";
            }
        }
        sendCommand(atoi(tokens[1].c_str()), msg);

    } if( (tokens[0].compare("CONNECT") == 0) ) {
        // We make the server connect to another server
        printf("CONNECT received\n");
        sendCommand(clientSocket, "Connecting to other server...\n");
        struct sockaddr_in servaddr;
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        struct hostent *he;
        he = gethostbyname(tokens[1].c_str());
        if(he == NULL)
        {
            perror("no such host");
            exit(0);
        }
        memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);
        servaddr.sin_port = htons(atoi(tokens[2].c_str()));

        int o_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(o_socket, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
            perror("Connect failed: ");
            exit(0);
        } else {
            std::cout << "Connected new server to: " << o_socket << std::endl;
        }

        // Add new client to the list of open sockets
        FD_SET(o_socket, openSockets);

        // And update the maximum file descriptor
        *maxfds = std::max(*maxfds, o_socket);

        // create a new client to store information.
        clients[o_socket] = new Client(o_socket);
    }
}

/* Commands other servers can issue */
void serverCommand(int clientSocket, fd_set *openSockets, int *maxfds, std::vector<std::string> tokens) {

    // Respond to the LISTSERVERS with our information and the 1-hop connected servers
    if(tokens[0].compare("LISTSERVERS") == 0)
    {
        std::string msg;
        msg = "SERVERS,";

        // Our info
        msg += serverID;
        msg += ",";
        myAddress = getIp();
        msg += myAddress;
        msg += ",";
        msg += std::to_string(myPort);
        msg += ";";

        // 1 hop servers
        for(auto const& elem : clients)
        {
            if(elem.second->GROUP_ID != "NoName" && elem.second->GROUP_ID != "Client") {
                msg += elem.second->GROUP_ID;
                msg += ",";
                msg += elem.second->HOST_IP;
                msg += ",";
                msg += elem.second->SERVPORT;
                msg += ";";
            }
        }
        // Save the name that the other server provided
        clients[clientSocket]->GROUP_ID = tokens[1];
        sendCommand(clientSocket, msg);

        // When we receive the response we will save the information to the corresponding clientSocket
    } else if (tokens[0].compare("SERVERS") == 0) {

        if(clients[clientSocket]->HOST_IP == "NoIP") {
            clients[clientSocket]->GROUP_ID = tokens[1];
            clients[clientSocket]->HOST_IP = tokens[2];
            clients[clientSocket]->SERVPORT = tokens[3];
            clients[clientSocket]->timeout = std::chrono::system_clock::now();
        }
    } else if (tokens[0].compare("SEND_MSG") == 0 ) {
        std::string msg = "From: ";
        msg += tokens[1];
        msg += " : ";
        msg += tokens[3];
        msgLog[tokens[2]].push_back(msg);
    } else if ( tokens[0].compare("GET_MSG") == 0 ) {
        for(int i = 0; i < msgLog[tokens[1]].size(); i++) {
            sendCommand(clientSocket, msgLog[tokens[1]][i]);
        }
    }
    else {
        printf("Unknown command\n");
    }
}

void runCommand(int clientSocket, fd_set *openSockets, int *maxfds, char *buffer) {
    // Split command from client into tokens for parsing
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream stream(buffer);
    while( getline(stream, token, ',')) {
        std::stringstream deepstream(token);
        std::string deeptoken;
        while ( getline(deepstream, deeptoken, ';') ) {
            tokens.push_back(deeptoken);
        }
    }

    // Our password check for issuing client commands
    if((tokens[0].compare("PASS") == 0) && tokens.size() == 2) {
        if(tokens[1].compare("100!") == 0) {
            mainClient = clientSocket;
            std::string msg = "Welcome master, send me your commands";
            send(clientSocket, msg.c_str(), msg.length(), 0);
        } else {
            std::string msg = "Wrong password! >:(";
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }
        return;
    }

    // Deceide whether it's a client or server command
    if ( clientSocket == mainClient ) {
        std::cout << "Client command: " << std::endl;
        // Log with timestamp
        logOut(buffer);
        clientCommand(clientSocket, openSockets, maxfds, tokens);

    } else {
        std::cout << "Server command: " << std::endl;
        // Log with timestamp
        logOut(buffer);
        serverCommand(clientSocket, openSockets, maxfds, tokens);
    }
    bzero(buffer, strlen(buffer));
}

int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients
    /* Writing to log file */
    //std::freopen("serverLog.log", "w", stdout);

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to
    myPort = atoi(argv[1]);
    std::cout << myPort << std::endl;
    listenSock = open_socket(myPort);
    printf("Listening on port: %d\n", myPort);

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else
        // Add listen socket to socket set we are monitoring
    {
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while(!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, &tv);
        if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if(FD_ISSET(listenSock, &readSockets))
            {
                clientSock = accept(listenSock, (struct sockaddr *)&client,&clientLen);
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);
                maxfds = std::max(maxfds, clientSock);
                clients[clientSock] = new Client(clientSock);
                n--;

                std::cout << "Client connected on server: " << clientSock << std::endl;

            }

            // Now check for commands from clients
            while(n-- > 0)
            {
                for(auto const& pair : clients)
                {
                    Client *client = pair.second;

                    if(FD_ISSET(client->sock, &readSockets)) {

                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                            printf("Client closed connection: %d", client->sock);
                            close(client->sock);

                            closeClient(client->sock, &openSockets, &maxfds);
                        } else {
                            // Check if correct SOI
                            if (buffer[0] == 1) {
                                int n = strlen(buffer);
                                //std::cout << buffer << std::endl;
                                memmove(buffer - 1, buffer, n);
                                buffer[n-2] = '\0';
                                buffer[n-1] = '\0';

                                runCommand(client->sock, &openSockets, &maxfds, buffer);

                            } else {
                                std::string dropped = "Wrong Start Of Input (must be: 0x01)\n";
                                sendCommand(client->sock, dropped);
                            }
                        }
                    }
                }

            }
        }
        for(auto const& elem : clients) {
            if(elem.second->GROUP_ID == "NoName" && elem.second->attempts < 4) {
                // When someone connects, reply with a LISTSERVERS
                sendCommand(elem.second->sock, "LISTSERVERS," + serverID);
                elem.second->attempts += 1;
            }

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::chrono::duration<double> diff = now - elem.second->timeout;
            if( diff.count() > 60) {
                sendCommand(elem.second->sock, "KEEPALIVE,0");
                elem.second->timeout = now;
            }
        }
    }
}
