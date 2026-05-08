#pragma once
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cassert>
#include <random>
#include <iostream>
#include <sstream>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
//  Board constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int ROWS = 14;
static constexpr int COLS = 14;
static constexpr int NUM_PLAYERS = 4;
static constexpr int INF = 1'000'000;

// ─────────────────────────────────────────────────────────────────────────────
//  Piece & Player types
// ─────────────────────────────────────────────────────────────────────────────
enum PieceType : uint8_t { NONE=0, P, N, B, R, Q, K };
enum Color     : uint8_t { NO_COLOR=0, RED=1, BLACK=2, GREEN=3, BLUE=4 };

// Piece: 6 bits type + 3 bits color packed in a byte
struct Piece {
    PieceType type  = NONE;
    Color     color = NO_COLOR;

    bool empty() const { return type == NONE; }
    bool operator==(const Piece& o) const { return type==o.type && color==o.color; }
    bool operator!=(const Piece& o) const { return !(*this==o); }
};

static constexpr Piece NO_PIECE = {NONE, NO_COLOR};

// ─────────────────────────────────────────────────────────────────────────────
//  Square helpers
// ─────────────────────────────────────────────────────────────────────────────
struct Sq { int r, c; };
inline bool operator==(Sq a, Sq b){ return a.r==b.r && a.c==b.c; }
inline bool inBounds(int r, int c){
    if (r<0||r>=ROWS||c<0||c>=COLS) return false;
    // Cut the 3×3 corners
    if (r<3 && c<3)   return false;
    if (r<3 && c>10)  return false;
    if (r>10 && c<3)  return false;
    if (r>10 && c>10) return false;
    return true;
}
inline bool inBounds(Sq s){ return inBounds(s.r, s.c); }

// ─────────────────────────────────────────────────────────────────────────────
//  Move
// ─────────────────────────────────────────────────────────────────────────────
struct Move {
    int8_t sr, sc, tr, tc;       // source & target
    PieceType promotion = NONE;  // if pawn promotes
    bool castleKingside  = false;
    bool castleQueenside = false;

    bool valid() const { return inBounds(sr,sc) && inBounds(tr,tc); }
    bool operator==(const Move& o) const {
        return sr==o.sr && sc==o.sc && tr==o.tr && tc==o.tc && promotion==o.promotion;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Score & elimination state
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerState {
    int  score       = 0;
    bool eliminated  = false;
    bool castledK    = false;   // has castled kingside (or lost right)
    bool castledQ    = false;
    bool kingMoved   = false;
    bool rookKMoved  = false;
    bool rookQMoved  = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Zobrist hashing
// ─────────────────────────────────────────────────────────────────────────────
namespace Zobrist {
    // [color 1-4][piece 1-6][row][col]
    extern uint64_t table[5][7][ROWS][COLS];
    extern uint64_t sideToMove[5]; // whose turn hash
    void init();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Board
// ─────────────────────────────────────────────────────────────────────────────
struct Board {
    Piece cells[ROWS][COLS];
    PlayerState ps[5];           // indexed by Color (1-4)
    int    turnOrder[4];         // active turn order (Color values)
    int    turnIdx = 0;          // index into turnOrder
    uint64_t hash = 0;

    void reset();

    Piece at(int r, int c) const {
        if (!inBounds(r,c)) return NO_PIECE;
        return cells[r][c];
    }
    Piece at(Sq s) const { return at(s.r, s.c); }
    void  set(int r, int c, Piece p);
    void  set(Sq s, Piece p){ set(s.r, s.c, p); }

    Color currentPlayer() const {
        // skip eliminated
        for(int i=0;i<4;i++){
            Color c = (Color)turnOrder[(turnIdx+i)%4];
            if(!ps[c].eliminated) return c;
        }
        return NO_COLOR;
    }

    Sq findKing(Color col) const;
    bool isInCheck(Color col) const;
    bool isCheckmated(Color col);
    bool isStalemate(Color col);

    // Generate pseudo-legal moves for a piece
    void genMovesFor(int r, int c, std::vector<Move>& out) const;
    // Generate all pseudo-legal moves for a color
    void genAllMoves(Color col, std::vector<Move>& out) const;
    // Generate fully legal moves (king not left/put in check)
    void legalMoves(Color col, std::vector<Move>& out);

    void applyMove(const Move& m);
    void undoMove(const Move& m, Board& saved); // copies saved back

    Board clone() const { return *this; }

    // Material count helpers
    int materialOf(Color col) const;
    int pieceCount(Color col) const;

    // Piece-square tables score for a piece at position
    static int pst(PieceType t, Color col, int r, int c);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Transposition table
// ─────────────────────────────────────────────────────────────────────────────
enum TTFlag : uint8_t { TT_EXACT=0, TT_LOWER, TT_UPPER };
struct TTEntry {
    uint64_t hash = 0;
    int      score = 0;
    int8_t   depth = 0;
    TTFlag   flag  = TT_EXACT;
    Move     best  = {0,0,0,0};
};
static constexpr size_t TT_SIZE = 1 << 22; // 4M entries
extern TTEntry g_tt[TT_SIZE];

// ─────────────────────────────────────────────────────────────────────────────
//  Evaluation
// ─────────────────────────────────────────────────────────────────────────────
namespace Eval {
    // Full static evaluation from the perspective of `aiColor`
    // Returns a score: higher = better for AI
    int evaluate(const Board& b, Color aiColor);

    // Mobility score for a color
    int mobilityScore(Board& b, Color col);

    // King safety
    int kingSafety(const Board& b, Color col);

    // Pawn structure
    int pawnStructure(const Board& b, Color col);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Search
// ─────────────────────────────────────────────────────────────────────────────
struct SearchResult {
    Move bestMove = {0,0,0,0};
    int  score    = 0;
    int  depth    = 0;
    long nodes    = 0;
};

class Engine {
public:
    Color aiColor = RED;          // AI always plays Red
    int   maxDepth = 5;
    long  timeLimit_ms = 5000;    // 5 seconds per move default

    SearchResult search(Board& b);

private:
    long nodes_ = 0;
    std::chrono::steady_clock::time_point deadline_;
    bool timesUp() const;

    // Paranoid search: AI maximises own score, opponents all minimise AI score
    int paranoidSearch(Board& b, int depth, int alpha, int beta, Color perspective);

    // Quiescence search
    int quiesce(Board& b, int alpha, int beta);

    // Move ordering: returns sorted moves (best first)
    void orderMoves(std::vector<Move>& moves, const Board& b, const Move& ttMove);

    // Static exchange evaluation (for move ordering captures)
    int see(const Board& b, Move m) const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Game manager (handles the full game loop / protocol)
// ─────────────────────────────────────────────────────────────────────────────
class Game {
public:
    Board board;
    Engine engine;

    void start();                   // enter interactive loop
    void printBoard() const;
    void printScores() const;
    std::string moveToStr(const Move& m) const;
    std::optional<Move> strToMove(const std::string& s, Color col) const;
    void printLegalMoves(Color col);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Piece values
// ─────────────────────────────────────────────────────────────────────────────
namespace PieceVal {
    static constexpr int val[7] = { 0, 100, 320, 330, 500, 900, 20000 };
    // Capture scoring: P=1, N/B=3, R=5, Q=9 (game points)
    static constexpr int pts[7] = { 0, 1,   3,   3,   5,   9,   0     };
    inline int get(PieceType t){ return val[(int)t]; }
}
