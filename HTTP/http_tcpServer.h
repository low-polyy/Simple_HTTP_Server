#ifndef HTTP_TCPSERVER_H
#define HTTP_TCPSERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>

#define MAX_BUFFER 30720;

class TCPServer
{
    public:
        TCPServer(std::string ip_address, int portno)
        : sIPAddress(ip_address), nPort(portno)
        {
            startServer();
            logMessage("Successfully created server.");
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

            int nBytesRead;
            while(true)
            {
                logMessage("===== Waiting for a new connection ===== \n\n\n");
                acceptConnection(mNewSocket);


                const int BUFFER_SIZE = MAX_BUFFER;
                char cBuffer[BUFFER_SIZE] = {0}; // initialize the buffer

                nBytesRead += recv(mSocket,(char*)&cBuffer,sizeof(cBuffer),0);
                if(nBytesRead < 0)
                    exitWithError("Failed to read bytes from client socket connection. Exiting.");
                
                std::ostringstream ss;
                ss << "----- Recieved request from client -----\n\n";
                logMessage(ss.str());

                sendResponse();
                close(mNewSocket);
            }
        }

    private:
        std::string sIPAddress; // *provided in constructor
        int nPort; // *provided in constructor
        int mSocket, mNewSocket;
        long lIncomingMessage;
        struct sockaddr_in mSocketAddress;
        std::string sServerMessage;

        void exitWithError(const std::string& s_message)
        {
            std::cerr << ">>[ERROR/(sys call failed)]: " << s_message << std::endl;
            exit(1);
        }

        void logMessage(const std::string& s_message)
        {
            std::cout << ">>[LOG]: " << s_message << std::endl;
        }

        int startServer()
        {
            // create and open socket
            mSocket = socket(AF_INET,SOCK_STREAM,0);
            if(mSocket < 0)
                exitWithError("Failed to create socket. Exiting.");

            // set fields in mSocketAddress
            mSocketAddress.sin_family = AF_INET;
            mSocketAddress.sin_addr.s_addr = inet_addr(sIPAddress.c_str()); // ip address
            mSocketAddress.sin_port = htons(nPort); // port number in network byte order

            // tie socket address to a given socket fd
            if(bind(mSocket, (struct sockaddr*)&mSocketAddress, sizeof(mSocketAddress)) < 0)
                exitWithError("Error on binding socket to local address. Exiting.");
        }

        void sendResponse()
        {
            int nBytesWritten;
            long lTotalBytesWritten {0};
            while(lTotalBytesWritten < sServerMessage.size())
            {
                nBytesWritten = write(mNewSocket,sServerMessage.c_str(),sServerMessage.size());
                if(nBytesWritten < 0)
                    break;
                lTotalBytesWritten += nBytesWritten;                            
            }

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
                ss << "Server failed tp accept incoming connection from ADDRESS: " 
                    << inet_ntoa(mSocketAddress.sin_addr) << " PORT: " 
                    << ntohs(mSocketAddress.sin_port);
                exitWithError(ss.str());
            }
        }


        void closeServer()
        {
            close(mSocket);
            close(mNewSocket);
            exit(0);

        }
};


#endif // !HTTP_TCPSERVER_H
