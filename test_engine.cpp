#include "chess4.hpp"
#include <iostream>
#include <cassert>

void test_initial_board() {
    Board b;
    b.reset();
    assert(b.currentPlayer() == RED);
    std::cout << "Initial board test passed.\n";
}

void test_ai_makes_move() {
    Board b;
    b.reset();
    Engine e;
    e.maxDepth = 2;
    SearchResult res = e.search(b);
    assert(res.bestMove.valid());
    std::cout << "AI move generation test passed.\n";
}

void test_is_attacked() {
    Board b;
    b.reset();
    assert(b.isAttacked(2, 7, BLACK));
    assert(b.isAttacked(2, 6, BLACK));
    assert(b.isAttacked(2, 8, BLACK));
    assert(!b.isAttacked(3, 7, BLACK));
    assert(b.isAttacked(2, 3, BLACK));
    assert(b.isAttacked(2, 5, BLACK));
    std::cout << "isAttacked test passed.\n";
}

int main() {
    Zobrist::init();
    test_initial_board();
    test_ai_makes_move();
    test_is_attacked();
    std::cout << "All tests passed!\n";
    return 0;
}
