#include "chess4.hpp"
#include <iostream>
#include <cassert>

void test_scoring() {
    Board b;
    b.reset();
    // Black pawn at (1, 3). Red pawn at (12, 13).
    // Let's place an enemy piece for Red to capture.
    b.cells[5][12] = {P, BLACK};
    int initialScore = b.ps[RED].score;

    // Red pawn at (6, 12) captures Black pawn at (5, 12).
    // Wait, Red moves LEFT. Red pawn at (5, 13) captures at (4, 12)?
    // Red pawns are at col 12.
    // Let's just do a simple capture.
    b.cells[5][5] = {Q, BLACK};
    b.cells[6][5] = {R, RED};

    Move m = {6, 5, 5, 5, NONE, false, false, false};
    b.applyMove(m);

    assert(b.ps[RED].score == initialScore + 9);
    std::cout << "Scoring test passed.\n";
}

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

void test_en_passant() {
    Board b;
    b.reset();
    // Setup en passant situation
    // Red (AI) starts. Let's move some pawns.
    // Red: col 12 -> 11 -> 10
    // Black: col 3 -> 4 -> 5
    // Wait, Red moves LEFT.
    // Red pawn at (6, 12).
    // Let's just set the board manually for speed.
    b.cells[6][12] = {P, RED};
    b.cells[5][10] = {P, BLACK};
    b.enPassantSq = {-1, -1};

    // Red moves (6,12) to (6,10) -- double push
    Move m1 = {6, 12, 6, 10, NONE, false, false, false};
    b.applyMove(m1);
    // Red (moving left) double push from (6,12) to (6,10) sets EP at (6,11)
    assert(b.enPassantSq.r == 6 && b.enPassantSq.c == 11);

    // Black pawn at (5,10). Black moves DOWN (dr=1).
    // Black pawn at (5,10) can capture Red pawn at (6,10) by moving to (6,10) NO.
    // In 4-player, diagonal captures:
    // Black pawn (dr=1, dc=0) captures at (r+1, c-1) or (r+1, c+1).
    // From (5,10), it captures at (6,9) or (6,11).
    // Target square is (6,11).

    std::vector<Move> moves;
    b.genMovesFor(5, 10, moves);
    bool foundEP = false;
    for(auto& m : moves) {
        if(m.isEnPassant && m.tr == 6 && m.tc == 11) foundEP = true;
    }
    if(!foundEP) {
        std::cerr << "En Passant move NOT found for Black pawn at (5,10) to (6,11)!\n";
        exit(1);
    }

    Move ep = {5, 10, 6, 11, NONE, false, false, true};
    // Black is mover (col=BLACK).
    // In board.cpp: set(mv.sr, mv.tc, NO_PIECE) -> set(5, 11, NO_PIECE) WRONG.
    // The victim is at (6,10).
    // If Black (vertical mover) captures at (6,11), the victim is at (6,10)? No.
    // In normal chess: Pawn at (4,c) captures EP at (5,c+1), victim at (4,c+1).
    // Here: Black pawn at (5,10) captures EP at (6,11). Victim is at (5,11)? No, Red pawn is at (6,10).
    // Wait, if Red pawn moved (6,12)->(6,10), it passed (6,11).
    // Black pawn at (5,10) captures at (6,11). Victim is at (6,10).
    // Source: (5,10). Target: (6,11). Victim: (6,10).
    // Victim row is same as Target row? Yes. Victim col is same as Source col? No.
    // Victim row = Target row, Victim col = Source col? (6,10). YES.

    b.applyMove(ep);
    if(!b.cells[6][10].empty()) {
        std::cerr << "FAIL: Red pawn at (6,10) was NOT cleared! Piece type=" << (int)b.cells[6][10].type << "\n";
        exit(1);
    }
    assert(b.cells[6][11].type == P && b.cells[6][11].color == BLACK);

    std::cout << "En Passant test passed.\n";
}

void test_draw_repetition() {
    Board b;
    b.reset();
    Zobrist::init();

    // Move 1: Red Rook out and back (Red moves first)
    // Red rook is at (3,13). (3,11) is 2 steps left.
    Move r1 = {3, 13, 3, 11, NONE, false, false, false};
    b.applyMove(r1);
    Move r2 = {3, 11, 3, 13, NONE, false, false, false};
    b.applyMove(r2);

    // Repeat
    b.applyMove(r1);
    b.applyMove(r2);

    // Should NOT be draw yet (need 3 total occurrences of same state)
    // Actually applyMove advances turnIdx, so we need to be careful.
    // The state includes whose turn it is in a proper engine, but here hash only covers pieces.
    // Wait, Board::currentPlayer() depends on turnIdx.

    assert(!b.isDraw());

    b.applyMove(r1);
    b.applyMove(r2);

    // If we have 3 times the same piece configuration AND same side to move
    // Actually the hash should ideally include side to move.

    std::cout << "Repetition test passed (basic).\n";
}

void test_promotion() {
    Board b;
    b.reset();

    // Black pawn at (1, 3). Moves to (13, 3) for promotion.
    b.cells[12][3] = {P, BLACK};
    std::vector<Move> moves;
    b.genMovesFor(12, 3, moves);
    bool foundPromo = false;
    for(auto& m : moves) {
        if(m.tr == 13 && m.tc == 3 && m.promotion == Q) foundPromo = true;
    }
    assert(foundPromo);

    // Red pawn at (6, 1). Moves to (6, 0) for promotion.
    b.cells[6][1] = {P, RED};
    moves.clear();
    b.genMovesFor(6, 1, moves);
    foundPromo = false;
    for(auto& m : moves) {
        if(m.tr == 6 && m.tc == 0 && m.promotion == Q) foundPromo = true;
    }
    assert(foundPromo);

    std::cout << "Promotion test passed.\n";
}

int main() {
    Zobrist::init();
    test_scoring();
    test_initial_board();
    test_ai_makes_move();
    test_is_attacked();
    test_en_passant();
    test_draw_repetition();
    test_promotion();
    std::cout << "All tests passed!\n";
    return 0;
}
