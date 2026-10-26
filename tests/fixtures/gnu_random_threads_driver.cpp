#include "gnu_random_threads.cpp"
#include <iostream>
#include <thread>
#include <vector>
int main() {
    for (unsigned seed : {0U, 0x80000000U, 0xffffffffU}) {
        seed_rng(seed);
        std::vector<std::thread> workers;
        for (int worker = 0; worker < 8; ++worker)
            workers.emplace_back([] {
                for (int i = 0; i < 100003; ++i)
                    read_rng();
            });
        for (auto& worker : workers)
            worker.join();
        for (int i = 0; i < 64; ++i)
            std::cout << read_rng() << '\n';
    }
}
