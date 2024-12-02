#include <chrono>
#include <iostream>
#include <thread>

/* this little code measures how long it takes to initalize and start an std::thread */

auto t0 = std::chrono::high_resolution_clock::now();

void thread_function()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - t0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Thread startup time: " << duration.count() << " microseconds" << std::endl;
}

int main()
{
    t0 = std::chrono::high_resolution_clock::now();
    std::thread t(thread_function);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - t0);
    std::cout << "Thread creation time: " << duration.count() << " microseconds" << std::endl;
    t.join();
    return 0;
}
