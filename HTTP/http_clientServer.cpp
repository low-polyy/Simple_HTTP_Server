#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>

#define MAX_BUFFER_SIZE 4096

class ClientServer
{
    public:
        ClientServer(std::string ip_address, int portno)
        : sIPAddress(ip_address), nPort(portno)
        {
            startClient();
            logMessage("Client successfully created.");
        }

        ~ClientServer()
        {
            closeClient();
        }

        void clientConnect()
        {
            if(connect(mSocket,(struct sockaddr*) &mSocketAddress, sizeof(mSocketAddress)) < 0)
                exitWithError("Error establishing connection with HTTP server. Exiting");
            else
            {
                logMessage("Successfully established a connection with the server.");
                sendRequest();
            }
        }
    private:
        std::string sIPAddress;
        std::uint16_t nPort;
        int mSocket;
        struct sockaddr_in mSocketAddress;
        struct hostent *mServer;
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

        int startClient()
        {
           // create and open socket
           mSocket = socket(AF_INET,SOCK_STREAM,0);
           if(mSocket < 0)
                exitWithError("Failed to create socket. Exiting.");

            // find server to connect to with the provided ip address
            mServer = gethostbyname(sIPAddress.c_str());
            if(mServer == NULL)
                exitWithError("Failed to find host. Exiting.");

            // set fields in socket 
            bzero((char*) &mSocketAddress, sizeof(mSocketAddress)); // initialize the address buffer
            mSocketAddress.sin_family = AF_INET;
            bcopy((char*)mServer->h_addr, (char*)&mSocketAddress.sin_addr.s_addr, mServer->h_length);
            mSocketAddress.sin_port = htons(nPort);
        }

        void sendRequest()
        {
            while(true)
            {
                int nBytesWritten, nBytesRead;
                char cBuffer[MAX_BUFFER_SIZE];
                // read message from std::in (just as a test for now...)
                std::cout << ">> Enter your message: ";
                std::string sData;
                std::getline(std::cin, sData);
                memset(&cBuffer, 0, sizeof(cBuffer)); // clear buffer by filling cBuffer with 0, initialize
                strcpy(cBuffer, sData.c_str()); // copy from sData to cBuffer

                nBytesWritten = write(mSocket, cBuffer, strlen(cBuffer));
                if(nBytesWritten < 0)
                    exitWithError("Error writing to socket. Exiting.");
                
                bzero(cBuffer,MAX_BUFFER_SIZE);
                nBytesRead = read(mSocket,cBuffer,MAX_BUFFER_SIZE);
                if(nBytesRead < 0)
                    exitWithError("Error reading response from server. Exiting.");
                logMessage(cBuffer);
            }
        }


        void closeClient()
        {
            close(mSocket);
            exit(0);
        }
};

int main()
{
    ClientServer myClient = ClientServer("127.0.0.1", 8080);
    myClient.clientConnect();
    return 0;
}

