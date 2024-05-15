#include <thread>
#include <iostream>

void testFunct(int x)
{
    std::cout << ">> Hello from thread" << std::endl;
    std::cout << ">> Argument(s) passed in: " << x  << std::endl;
}

int main()
{
    // pause main()
    // execute threaded function first before continuing on main()
    std::thread myThread(&testFunct, 100);
    myThread.join();
    std::cout << ">> Hello from main thread" << std::endl;

    return 0;
}