#include "chess4.hpp"
#include <iostream>
#include <chrono>

int main() {
    Board b;
    b.reset();
    Engine e;
    e.aiColor = RED;
    e.maxDepth = 3;
    e.timeLimit_ms = 100000;

    auto start = std::chrono::steady_clock::now();
    SearchResult res = e.search(b);
    auto end = std::chrono::steady_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Depth: " << res.depth << "\n";
    std::cout << "Nodes: " << res.nodes << "\n";
    std::cout << "Time: " << diff << "ms\n";
    if (diff > 0) {
        std::cout << "Nodes/sec: " << (res.nodes * 1000 / diff) << "\n";
    }

    return 0;
}
