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
#define THREAD_POOL_SIZE 5
#define MAX_EVENTS 10000

struct EventData
{
    // to be able to parse HTTP requests & such
    EventData()
    :nSocketfd(0),nLength(0),nCursor(0),cBuffer()
    {}

    int nSocketfd;
    std::size_t nLength;
    std::size_t nCursor;
    char cBuffer[MAX_BUFFER_SIZE];
};

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
            if(listen(mSocket,20) < 0)
                exitWithError("Failed to listen on given socket. Exiting.");
            std::ostringstream ss;
            ss << "\n *** Listening on ADDRESS: " << inet_ntoa(mSocketAddress.sin_addr)
                << " PORT: " << ntohs(mSocketAddress.sin_port)
                << "*** \n\n";
            logMessage(ss.str());

            epollSetup();
            mListenerThread = std::thread(&serverListen,this);
            for(int i{0}; i < THREAD_POOL_SIZE; ++i)
            {
                mWorkerThread[i] = std::thread(processEvents,this,i);
            }

            while(true)
            {
                //  == TODO: rewrite this. ==
                int nNumEvents = epoll_wait(nEpoll,mEpollEventsList,MAX_EVENTS,-1);
                for(int i{0}; i!=nNumEvents; ++i)
                {
                    if(mEpollEventsList[i].data.fd == mSocket)
                    {
                        logMessage("Server socket ready for reading operations. \n");
                        logMessage("===== Waiting for a new connection ===== \n\n\n");
                        acceptConnection(mNewSocket);

                        // add client socket to epoll
                        mEpollEvent.events = EPOLLIN | EPOLLET;
                        mEpollEvent.data.fd = mNewSocket;
                        epoll_ctl(nEpoll, EPOLL_CTL_ADD, mNewSocket, &mEpollEvent);

                        // create new thread to handle the client connection
                        std::cout << "Woooo vreate thread here \n";
                    }
                    else
                    {
                        handleClient();
                        mNewSocket = mEpollEventsList[i].data.fd;
                    }
                }
            }
            close(mSocket);
        }

    private:
        std::string sIPAddress; // *provided in constructor
        std::uint16_t nPort; // *provided in constructor
        int mSocket, mNewSocket;
        struct sockaddr_in mSocketAddress;
        std::string sServerMessage;

        std::thread mListenerThread;
        std::thread mWorkerThread[THREAD_POOL_SIZE];
        std::vector<std::thread> vThreads;
        int mWorkerEpoll[THREAD_POOL_SIZE];
        struct epoll_event mEpollEvent;
        struct epoll_event mEpollEventsList[MAX_EVENTS];
        struct epoll_event mWorkerEvents[THREAD_POOL_SIZE][MAX_EVENTS];

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

        void epollSetup()
        {
            // create epoll instance to monitor I/O on socket fd
            int nEpoll = epoll_create1(0);
            if(nEpoll < 0)
                exitWithError("Failed to create epoll instance. Exiting.");
            // add server's socket to epoll for monitoring read activities
            mEpollEvent.events = EPOLLIN;
            mEpollEvent.data.fd = mSocket;
            epoll_ctl(nEpoll, EPOLL_CTL_ADD, mSocket, &mEpollEvent);
        }

        void controlEpollEvent(int nEpollfd, int opt, int fd, std::uint32_t nEvents, void *data)
        {
            if(opt == EPOLL_CTL_DEL)
            {
                if(epoll_ctl(nEpollfd,opt,fd,nullptr) < 0)
                    exitWithError("Failed to remove the file descriptor. Exiting.");
            }
            else
            {
                epoll_event nEpollEvent;
                nEpollEvent.events = nEvents;
                nEpollEvent.data.ptr = data;
                if(epoll_ctl(nEpollfd, opt, fd, &nEpollEvent) < 0)
                    exitWithError("Failed to add the file descriptor. Exiting.");
            }
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

        void processEvents(int n_worker_ID)
        {
            int nEpoll = mWorkerEpoll[n_worker_ID];
            bool bActive = true;

            while(true)
            {
                if(!bActive)
                    std::this_thread::sleep_for(std::chrono::microseconds(mSleepTimes(mRand)));

                // blocks until events are available (fills mEpollEventsList with the events that occured)
                int numEvents = epoll_wait(nEpoll,mWorkerEvents[n_worker_ID],MAX_EVENTS,0);
                if(numEvents <= 0)
                {
                    bActive = false;
                    continue;
                }

                bActive = true;
                for(int i{0}; i < numEvents; ++i)
                {
                    const epoll_event &nCurrentEvent = mWorkerEvents[n_worker_ID][i];
                    if((nCurrentEvent.events & EPOLLHUP) ||
                    nCurrentEvent.events & EPOLLERR)
                    {
                        epoll_ctl(nEpoll,EPOLL_CTL_DEL,mNewSocket,&mEpollEvent);
                        close(mNewSocket);
                    }


                }
                
            }
        }

        void serverListen()
        {
            EventData *clientData;
            bool bActive = true;
            int nCurrWorker = 0;
            socklen_t nSocketAddress_len = sizeof(mSocketAddress);

            while(true)
            {
                if(!bActive)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(mSleepTimes(mRand)));
                }
                
                // accept connection from client
                mNewSocket = accept(mSocket, (sockaddr*)&mSocketAddress,&nSocketAddress_len);
                if(mNewSocket < 0)
                {
                    std::ostringstream ss;
                    ss << "Server failed to accept incoming connection from ADDRESS: " 
                        << inet_ntoa(mSocketAddress.sin_addr) << " PORT: " 
                        << ntohs(mSocketAddress.sin_port);
                    bActive = false;
                    continue;
                }
                bActive = true;
                clientData = new EventData();
                clientData->nSocketfd = mNewSocket;
                logMessage("Client successfully connected!");

                epoll_ctl(mWorkerEpoll[nCurrWorker], EPOLL_CTL_ADD, mNewSocket, &mEpollEvent);
                nCurrWorker ++;
                if(nCurrWorker == THREAD_POOL_SIZE)
                    nCurrWorker = 0;
                
            }
        }
        

        void acceptConnection(int &new_socket)
        {
            //--> continue to recieving bytes
        }

        void handleClient()
        {
            int nBytesRead;
            char cBuffer[MAX_BUFFER_SIZE];

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
