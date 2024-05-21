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
#include <vector>
#include <random>

#define MAX_BUFFER_SIZE 4096

class TCPServer
{
    public:
        TCPServer(std::string ip_address, int portno)
        : sIPAddress(ip_address), nPort(portno), 
        mRand(std::chrono::steady_clock::now().time_since_epoch().count()),
        mSleepTimes(10,100)
        {
        
            socketSetup(); // start server
            logMessage("----- Successfully established socket on server -----.");
        }

        ~TCPServer()
        {
            closeServer();
        }


        void startServer()
        {
            // have the server listen for incoming client connections
            if(listen(mSocket,20) < 0)
                exitWithError("Failed to listen on given socket. Exiting.");
            std::ostringstream ss;
            ss << "\n *** Listening on ADDRESS: " << inet_ntoa(mSocketAddress.sin_addr)
                << " PORT: " << ntohs(mSocketAddress.sin_port)
                << "*** \n\n";
            logMessage(ss.str());

            while(true)
            {
                logMessage("===== Waiting for a new connection ===== \n\n\n");
                acceptConnection(mNewSocket);
                mPID = fork();
                if(mPID < 0)
                    exitWithError("Error on fork. Exiting.");
                if(mPID == 0)
                {
                    sendAndRecieve();
                }
                else
                    close(mNewSocket);
            }
            // terminate connection
            close(mSocket);
        }

    private:
        std::string sIPAddress; // *provided in constructor
        std::uint16_t nPort; // *provided in constructor
        int mSocket, mNewSocket;
        struct sockaddr_in mSocketAddress;
        std::string sServerMessage;
        int mPID;

        std::mt19937 mRand;
        std::uniform_int_distribution<int> mSleepTimes;


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

            // bind socket address to a given socket fd
            if(bind(mSocket, (struct sockaddr*)&mSocketAddress, sizeof(mSocketAddress)) < 0)
                exitWithError("Error on binding socket to local address. Exiting.");

            return mSocket;
        }


        void sendAndRecieve()
        {
            char cBuffer[MAX_BUFFER_SIZE];
            int nBytesWritten, nBytesRead;
            long lTotalBytesWritten {0};
            while(true)
            {
                // == recieve ==
                // client successfully connects to the server...
                bzero(cBuffer,MAX_BUFFER_SIZE); // initialize the buffer
                nBytesRead += recv(mNewSocket,(char*)&cBuffer,sizeof(cBuffer),0); // read from new socket fd
                if(nBytesRead < 0)
                    exitWithError("Failed to read bytes from client socket connection. Exiting.");
                std::ostringstream ss;
                ss << "----- Recieved request from client -----\n\n";
                logMessage(cBuffer);
                logMessage(ss.str());

                // == send ==
                bzero(cBuffer,MAX_BUFFER_SIZE);
                nBytesWritten = send(mSocket,(char*)&cBuffer,sizeof(cBuffer),0);
                if(nBytesWritten < 0)
                    exitWithError("Failed to write bytes to socket. Exiting.");
                lTotalBytesWritten += nBytesWritten;
                if(nBytesWritten == sServerMessage.size())
                    logMessage(">> Server response sent to client. \n\n");
                else
                    logMessage(">> Error sending response to client. \n\n");                           
            }
        }


        void acceptConnection(int &new_socket)
        {
            socklen_t nSocketAddress_len = sizeof(mSocketAddress);
            // accept connection from client
            mNewSocket = accept(mSocket, (sockaddr*)&mSocketAddress,&nSocketAddress_len);
            if(mSocket < 0)
            {
                std::ostringstream ss;
                ss << "Server failed to accept incoming connection from ADDRESS: " 
                    << inet_ntoa(mSocketAddress.sin_addr) << " PORT: " 
                    << ntohs(mSocketAddress.sin_port);
                exitWithError(ss.str());
            }
            logMessage("Client successfully connected!");
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
    myServer.startServer();
    return 0;
}
