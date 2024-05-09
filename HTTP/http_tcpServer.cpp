#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <pthread.h>
#include <thread>

#define MAX_BUFFER_SIZE 4096
#define THREAD_POOL_SIZE 5
#define MAX_EVENTS 10000

class TCPServer
{
    public:
        TCPServer(std::string ip_address, int portno)
        : sIPAddress(ip_address), nPort(portno)
        {
            socketSetup(); // start server
            logMessage("----- Successfully established socket on server -----.");
        }

        ~TCPServer()
        {
            closeServer();
        }

        void startListen()
        {
            if(listen(mSocket,20) < 0)
                exitWithError("Failed to listen on given socket. Exiting.");
            std::ostringstream ss;
            ss << "\n *** Listening on ADDRESS: " << inet_ntoa(mSocketAddress.sin_addr)
                << " PORT: " << ntohs(mSocketAddress.sin_port)
                << "*** \n\n";
            logMessage(ss.str());

            // create epoll instance to monitor I/O on socket fd
            int nEpoll = epoll_create1(0);
            if(nEpoll < 0)
                exitWithError("Failed to create epoll instance. Exiting.");
            // add server's socket to epoll for monitoring read activities
            mEpollEvent.events = EPOLLIN;
            mEpollEvent.data.fd = mSocket;
            epoll_ctl(nEpoll,EPOLL_CTL_ADD,mSocket,&mEpollEvent);


            int nBytesRead;
            char cBuffer[MAX_BUFFER_SIZE];
            while(true)
            {
                logMessage("===== Waiting for a new connection ===== \n\n\n");
                acceptConnection(mNewSocket);

                mPID = fork();
                if(mPID < 0)
                    exitWithError("Error on fork. Exiting.");
                if(mPID == 0)
                {
                    bzero(cBuffer,MAX_BUFFER_SIZE); // initialize the buffer
                    nBytesRead = read(mNewSocket,(char*)&cBuffer,sizeof(cBuffer));
                    if(nBytesRead < 0)
                        exitWithError("Failed to read bytes from client socket connection. Exiting.");

                    std::ostringstream ss;
                    ss << "----- Recieved request from client -----\n\n";
                
                    logMessage(cBuffer);
                    logMessage(ss.str());
                
                    bzero(cBuffer,MAX_BUFFER_SIZE);
                    sendResponse();
                }
                else
                    close(mNewSocket);
            }
            close(mSocket);
        }

    private:
        std::string sIPAddress; // *provided in constructor
        std::uint16_t nPort; // *provided in constructor
        int mSocket, mNewSocket;
        int mPID;
        struct sockaddr_in mSocketAddress;
        std::string sServerMessage;

        std::thread mListenerThread;
        std::thread mWorkerThread[THREAD_POOL_SIZE];
        int mWorkerEpoll[THREAD_POOL_SIZE];
        struct epoll_event mEpollEvent;




        void exitWithError(const std::string& s_message)
        {
            std::cerr << ">>[ERROR/(sys call failed)]: " << s_message << std::endl;
            exit(1);
        }

        void logMessage(const std::string& s_message)
        {
            std::cout << ">>[LOG]: " << s_message << std::endl;
        }

        int socketSetup()
        {
            // create and open socket
            mSocket = socket(AF_INET,SOCK_STREAM,0);
            if(mSocket < 0)
                exitWithError("Failed to create socket. Exiting.");

            // set fields in mSocketAddress
            bzero((char*) &mSocketAddress, sizeof(mSocketAddress)); // initialize the address buffer
            mSocketAddress.sin_family = AF_INET;
            mSocketAddress.sin_addr.s_addr = inet_addr(sIPAddress.c_str()); // ip address
            mSocketAddress.sin_port = htons(nPort); // port number in network byte order

            // tie socket address to a given socket fd
            if(bind(mSocket, (struct sockaddr*)&mSocketAddress, sizeof(mSocketAddress)) < 0)
                exitWithError("Error on binding socket to local address. Exiting.");

            return mSocket;
            // --> continue by listen()
        }

        void sendResponse()
        {
            int nBytesWritten;
            long lTotalBytesWritten {0};
            
            nBytesWritten = write(mNewSocket,sServerMessage.c_str(),sServerMessage.size());
            if(nBytesWritten < 0)
                exitWithError("Failed to write bytes to socket. Exiting.");
            lTotalBytesWritten += nBytesWritten;                            

            if(nBytesWritten == sServerMessage.size())
            {
                logMessage("----- Server response sent to client ------\n\n");
            }
            else
            {
                logMessage("----- Error sending response to client -----\n\n");
            }
        }


        void acceptConnection(int &new_socket)
        {
            socklen_t nSocketAddress_len = sizeof(mSocketAddress); 
            mNewSocket = accept(mSocket, (sockaddr*)&mSocketAddress,&nSocketAddress_len);
            if(mNewSocket < 0)
            {
                std::ostringstream ss;
                ss << "Server failed to accept incoming connection from ADDRESS: " 
                    << inet_ntoa(mSocketAddress.sin_addr) << " PORT: " 
                    << ntohs(mSocketAddress.sin_port);
                exitWithError(ss.str());
            }
            logMessage("Client successfully connected!");

            //--> continue to recieving bytes
        }


        void closeServer()
        {
            close(mSocket);
            close(mNewSocket);
            exit(0);

        }
};

int main()
{
    TCPServer myServer = TCPServer("127.0.0.1",8080);
    myServer.startListen();
    return 0;
}
