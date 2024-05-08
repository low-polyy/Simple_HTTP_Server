#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void exitWithError(const std::string& s_message)
{
    std::cerr << ">> [ERROR/(sys call failed)]: " << s_message << std::endl;
    exit(1);
}

void logMessage(const std::string& s_message)
{
    std::cout << ">> [LOG]: " << s_message << std::endl;
}

int socketSetup()
{
    
}