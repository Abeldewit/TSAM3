//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
// scp -r TSAM3 hartmann14@skel.ru.is:/home/hir.is/hartmann14/tsam
///130.208.243.61
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>

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
    int sock;              // socket of client connection
    std::string name;      // Limit length of name of client's user
    std::string GROUP_ID;  // Group ID of the server
    std::string HOST_IP;   // IP address of client*/
    std::string SERVPORT;          // Port of the server

    int attempts = 0;
    std::chrono::seconds timeout;

    Client(int socket) : sock(socket){}

    ~Client(){}            // Virtual destructor defined for base class
};



/// Global var - Added, string ServerID <-- ID our server?
std::string serverID = "V_GROUP100";


// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients; // Lookup table for per Client information
int myPort;
sockaddr myAddress;

int mainClient = -1;

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

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
        perror("Failed to open socket");
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

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

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

int sendCommand(int clientSocket, std::string msg) {
    if(msg[msg.length()-1] == 0x0a) {
        msg.pop_back();
    }

    int n = msg.length();
    char buffer[n+2];
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, msg.c_str());
    memmove(buffer + 1, buffer, sizeof(buffer));
    buffer[0] = 1;
    buffer[n+1] = 4;

    if(send(clientSocket, buffer, sizeof(buffer), 0) > 0) {
        return 0;
    } else {
        return 1;
    }
}

// When our client is logged in these commands will be active
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, std::vector<std::string> tokens) {

    if( (tokens[0].compare("GETMSG") == 0) && tokens.size() == 2) {
        // We try to receive a message from someone
        std::cout << "GETMSG received" << std::endl;

    } else if( (tokens[0].compare("SENDMSG") == 0) && tokens.size() == 3) {
        // We send a message to someone
        sendCommand(atoi(tokens[1].c_str()), tokens[2]);
        std::cout << "SENDMSG received" << std::endl;

    } else if( (tokens[0].compare("LISTSERVERS") == 0)) {
        // We list all the servers that our own server is connected to

        std::string msg;
        for(auto const& elem : clients)
        {
            msg += elem.second->GROUP_ID;
            msg +=elem.second->HOST_IP;
            msg +=elem.second->SERVPORT;
        }
        sendCommand(clientSocket, msg);
        /// sendCommand(clientSocket, msg);


    } else if( (tokens[0].compare("CONNECT") == 0) ) {
        // We force the server to connect to another server
        std::cout << "CONNECT received" << std::endl;
        sendCommand(clientSocket, "Connecting to other server...");
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

        std::string msg;
        if (tokens.size() == 4) {
            msg = "CONNECT ";
            msg += tokens[3];
        } else {
            msg = "CONNECT V_GROUP_100";
        }

        Client *newServer = new Client(o_socket);
        newServer->HOST_IP = tokens[1];
        newServer->SERVPORT = tokens[2];
        newServer->GROUP_ID = serverID;
        clients.emplace(o_socket, newServer);

        sendCommand(o_socket, msg);

    }

}

void serverCommand(int clientSocket, fd_set *openSockets, int *maxfds, std::vector<std::string> tokens) {
    if((tokens[0].compare("CONNECT") == 0) && (tokens.size() == 2))
    {
        clients[clientSocket]->name = tokens[1];
        std::string msg = "Connected ";
        msg += tokens[1];
        sendCommand(clientSocket, msg);
    }
    else if(tokens[0].compare("LEAVE") == 0)
    {
        // Close the socket, and leave the socket handling
        // code to deal with tidying up clients etc. when
        // select() detects the OS has torn down the connection.

        closeClient(clientSocket, openSockets, maxfds);
    }
    else if(tokens[0].compare("WHO") == 0)
    {
        std::cout << "Who is logged on" << std::endl;
        std::string msg;

        for(auto const& names : clients)
        {
            msg += names.second->name + ",";

        }
        // Reducing the msg length by 1 loses the excess "," - which
        // granted is totally cheating.
        sendCommand(clientSocket, msg);

    }
        // This is slightly fragile, since it's relying on the order
        // of evaluation of the if statement.
    else if((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0))
    {
        std::string msg;
        for(auto i = tokens.begin()+2;i != tokens.end();i++)
        {
            msg += *i + " ";
        }

        for(auto const& pair : clients)
        {
            sendCommand(pair.second->sock, msg);
        }
    }
    else if(tokens[0].compare("MSG") == 0)
    {
        for(auto const& pair : clients)
        {
            if(pair.second->name.compare(tokens[1]) == 0)
            {
                std::string msg;
                for(auto i = tokens.begin()+2;i != tokens.end();i++)
                {
                    msg += *i + " ";
                }
                sendCommand(pair.second->sock, msg);
            }
        }
    }
    else if(tokens[0].compare("LISTSERVERS") == 0)
    {
        std::string msg;

        for(auto const& elem : clients)
        {

            msg += elem.second->GROUP_ID;
            msg += ",";
            msg +=elem.second->HOST_IP;
            msg += ",";
            msg +=elem.second->SERVPORT;
        }
        sendCommand(clientSocket, msg);


    }
    else if(tokens[0].compare("SEECLIENTS") == 0)
    {

        std::string msg;
        socklen_t len;
        struct sockaddr_storage addr;

        len = sizeof addr;
        char ipstr[1024];
        //getpeername
        getpeername(clientSocket, (struct sockaddr*)&addr, &len);
        struct sockaddr_in *clientS = (struct sockaddr_in *)&addr;
        int port = ntohs(clientS->sin_port);
        inet_ntop(AF_INET, &clientS->sin_addr, ipstr, sizeof ipstr);
        for(auto const& elem : clients)
        {
            elem.second->GROUP_ID = serverID;
            elem.second->HOST_IP = ipstr;
            elem.second->SERVPORT = port;

            msg += serverID + "," + ipstr + ":" + std::to_string(port) + "\n";


            //  send(clientSocket, msg.c_str(), msg.length(), 0);
        }
        sendCommand(clientSocket, msg);


    }

}

void runCommand(int clientSocket, fd_set *openSockets, int *maxfds, char *buffer) {
    std::vector<std::string> tokens;
    std::string token;
    // Split command from client into tokens for parsing
    std::cout.flush();
    std::stringstream stream(buffer);

    while(stream >> token)
        tokens.push_back(token);

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

    if ( clientSocket == mainClient ) {
        // Do our client stuff
        std::cout << "Client command:" << buffer;
        clientCommand(clientSocket, openSockets, maxfds, tokens);

    } else {
        // Do server stuff
        std::cout << "Server command:" << buffer << "\n";
        serverCommand(clientSocket, openSockets, maxfds, tokens);

    }

    std::cout.flush();
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

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

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
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

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
                clientSock = accept(listenSock, (struct sockaddr *)&client,
                                    &clientLen);
                std::string msg = "Welcome to the triple digits! Please verify yourself with CONNECT";
                sendCommand(clientSock, msg);
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);

                // create a new client to store information.
                clients[clientSock] = new Client(clientSock);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            while(n-- > 0)
            {
                for(auto const& pair : clients)
                {
                    Client *client = pair.second;

                    if(client->attempts > 2) {
                        printf("Too many attempts: %d", client->sock);

//                      close(client->sock);
//                      closeClient(client->sock, &openSockets, &maxfds);
                    }

                    if(FD_ISSET(client->sock, &readSockets)) {
                        // recv() == 0 means client has closed connection
                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                            printf("Client closed connection: %d", client->sock);
                            close(client->sock);

                            closeClient(client->sock, &openSockets, &maxfds);

                        }
                            // We don't check for -1 (nothing received) because select()
                            // only triggers if there is something on the socket for us.
                        else {
                            // Check if correct SOI
                            if (buffer[0] == 1) {
                                std::cout << buffer << std::endl;
                                int n = strlen(buffer);
                                memmove(buffer - 1, buffer, n);
                                buffer[n-2] = '\0';
                                buffer[n-1] = '\0';
                                runCommand(client->sock, &openSockets, &maxfds,
                                           buffer);
                            } else {
                                if ( client->attempts < 3) {
                                    std::string dropped = "Wrong Start Of Input (must be: 0x01)\n";
                                    sendCommand(client->sock, dropped);
                                    client->attempts += 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        /* This is me trying to close a connection */
//        for(auto const& pair : clients) {
//            Client *client = pair.second;
//            if ( client->attempts > 3 ) {
//                close(client->sock);
//                closeClient(client->sock, &openSockets, &maxfds);
//            }
//        }
    }
}
