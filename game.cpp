#include "chess4.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  ANSI colours for terminal display
// ─────────────────────────────────────────────────────────────────────────────
namespace Ansi {
    const char* RESET  = "\033[0m";
    const char* BOLD   = "\033[1m";
    const char* DIM    = "\033[2m";

    // Piece backgrounds
    const char* RED_BG    = "\033[41m";
    const char* BLACK_BG  = "\033[40m";
    const char* GREEN_BG  = "\033[42m";
    const char* BLUE_BG   = "\033[44m";

    // Board cell backgrounds
    const char* CELL_LIGHT = "\033[48;5;153m";   // light blue
    const char* CELL_DARK  = "\033[48;5;111m";   // slightly darker blue
    const char* CELL_DEAD  = "\033[48;5;234m";   // dark grey (corner)

    // Text colours
    const char* WHITE_FG   = "\033[97m";
    const char* YELLOW_FG  = "\033[93m";
    const char* CYAN_FG    = "\033[96m";
    const char* MAGENTA_FG = "\033[95m";

    const char* pieceBg(Color c){
        switch(c){
            case RED:   return "\033[48;5;196m\033[97m";
            case BLACK: return "\033[48;5;232m\033[97m";
            case GREEN: return "\033[48;5;34m\033[97m";
            case BLUE:  return "\033[48;5;21m\033[97m";
            default:    return "";
        }
    }
    const char* playerColor(Color c){
        switch(c){
            case RED:   return "\033[91m";
            case BLACK: return "\033[90m";
            case GREEN: return "\033[92m";
            case BLUE:  return "\033[94m";
            default:    return "\033[0m";
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Piece symbol
// ─────────────────────────────────────────────────────────────────────────────
static char pieceChar(PieceType t){
    switch(t){
        case P: return 'P'; case N: return 'N'; case B: return 'B';
        case R: return 'R'; case Q: return 'Q'; case K: return 'K';
        default: return '.';
    }
}
static const char* colorName(Color c){
    switch(c){
        case RED:   return "Red";
        case BLACK: return "Black";
        case GREEN: return "Green";
        case BLUE:  return "Blue";
        default:    return "None";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Board printer  (14×14 with row/col labels)
// ─────────────────────────────────────────────────────────────────────────────
void Game::printBoard() const {
    std::cout << "\n";
    // Col headers
    std::cout << "    ";
    for(int c=0;c<COLS;c++) std::cout << std::setw(3) << c;
    std::cout << "\n";
    std::cout << "    ";
    for(int c=0;c<COLS;c++) std::cout << "---";
    std::cout << "\n";

    for(int r=0;r<ROWS;r++){
        std::cout << std::setw(2) << r << " |";
        for(int c=0;c<COLS;c++){
            if(!inBounds(r,c)){
                std::cout << Ansi::CELL_DEAD << "   " << Ansi::RESET;
                continue;
            }
            const char* cellBg = ((r+c)&1) ? Ansi::CELL_DARK : Ansi::CELL_LIGHT;
            Piece p = board.at(r,c);
            if(p.empty()){
                std::cout << cellBg << "   " << Ansi::RESET;
            } else {
                std::cout << Ansi::pieceBg(p.color)
                          << " " << pieceChar(p.type) << " "
                          << Ansi::RESET;
            }
        }
        std::cout << "| " << r << "\n";
    }
    std::cout << "    ";
    for(int c=0;c<COLS;c++) std::cout << "---";
    std::cout << "\n    ";
    for(int c=0;c<COLS;c++) std::cout << std::setw(3) << c;
    std::cout << "\n\n";
}

void Game::printScores() const {
    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout <<   "║            SCORES & STATUS           ║\n";
    std::cout <<   "╠══════════════════════════════════════╣\n";
    Color order[4]={RED,BLACK,GREEN,BLUE};
    for(Color c: order){
        auto& st=board.ps[c];
        std::cout << "║ " << Ansi::playerColor(c) << Ansi::BOLD
                  << std::setw(6) << colorName(c) << Ansi::RESET
                  << " │ Points: " << std::setw(4) << st.score
                  << " │ " << (st.eliminated ? "\033[9mEliminated\033[0m" : "Active    ")
                  << " ║\n";
    }
    std::cout << "╚══════════════════════════════════════╝\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Move encoding:  "r c r c"   e.g. "3 12 3 11"   (row col row col)
//  Also accept algebraic shorthand:  "e2e4" style where col is a-n, row is 1-14
// ─────────────────────────────────────────────────────────────────────────────
std::string Game::moveToStr(const Move& m) const {
    std::ostringstream oss;
    // Column as letter a-n
    char fc = 'a' + m.sc;
    char tc = 'a' + m.tc;
    oss << fc << (m.sr+1) << tc << (m.tr+1);
    if(m.promotion!=NONE) oss << (char)std::tolower(pieceChar(m.promotion));
    return oss.str();
}

std::optional<Move> Game::strToMove(const std::string& s, Color /*col*/) const {
    // Accept two formats:
    // 1) "r c r c"  e.g. "3 12 4 12"
    // 2) "a1b2"     algebraic
    std::istringstream iss(s);
    Move m{};

    // Try numeric first
    int a,b,c,d;
    if(iss >> a >> b >> c >> d){
        m.sr=(int8_t)a; m.sc=(int8_t)b;
        m.tr=(int8_t)c; m.tc=(int8_t)d;
        if(inBounds(m.sr,m.sc)&&inBounds(m.tr,m.tc)) return m;
        return std::nullopt;
    }

    // Try algebraic e.g. "a1b2" or "a1b2q"
    iss.clear(); iss.str(s);
    std::string tok; iss >> tok;
    if(tok.size()>=4){
        int sc2 = std::tolower(tok[0])-'a';
        int sr2 = std::stoi(tok.substr(1,tok.size()>=5?(tok.size()-3):1))-1;
        // find where second col letter starts
        size_t p2=1; while(p2<tok.size()&&std::isdigit(tok[p2])) p2++;
        if(p2>=tok.size()) return std::nullopt;
        int tc2 = std::tolower(tok[p2])-'a';
        int tr2 = std::stoi(tok.substr(p2+1))-1;
        m.sr=(int8_t)sr2; m.sc=(int8_t)sc2;
        m.tr=(int8_t)tr2; m.tc=(int8_t)tc2;
        // promotion?
        char last=tok.back();
        if(last=='q'||last=='r'||last=='b'||last=='n'){
            switch(last){
                case 'q': m.promotion=Q; break;
                case 'r': m.promotion=R; break;
                case 'b': m.promotion=B; break;
                case 'n': m.promotion=N; break;
            }
        }
        if(inBounds(m.sr,m.sc)&&inBounds(m.tr,m.tc)) return m;
    }
    return std::nullopt;
}

void Game::printLegalMoves(Color col){
    std::vector<Move> moves;
    board.legalMoves(col, moves);
    if(moves.empty()){
        std::cout << "  (no legal moves)\n";
        return;
    }
    int n=0;
    for(auto& m:moves){
        Piece src=board.at(m.sr,m.sc);
        Piece cap=board.at(m.tr,m.tc);
        std::cout << "  " << std::setw(3) << ++n << ". "
                  << moveToStr(m)
                  << "  (" << pieceChar(src.type)
                  << " " << (int)m.sr << "," << (int)m.sc
                  << " → " << (int)m.tr << "," << (int)m.tc;
        if(!cap.empty()) std::cout << " x" << colorName(cap.color)[0] << pieceChar(cap.type);
        if(m.promotion!=NONE) std::cout << "=" << pieceChar(m.promotion);
        if(m.castleKingside)  std::cout << " O-O";
        if(m.castleQueenside) std::cout << " O-O-O";
        std::cout << ")\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Print help
// ─────────────────────────────────────────────────────────────────────────────
static void printHelp(){
    std::cout << "\n"
        << Ansi::CYAN_FG << "Commands:\n" << Ansi::RESET
        << "  <move>       Make a move.  Formats:\n"
        << "               Numeric:    \"row col row col\"   e.g.  3 12 3 11\n"
        << "               Algebraic:  \"a1b2\"              e.g.  m4l4\n"
        << "  moves        List all legal moves for current player\n"
        << "  board        Redisplay the board\n"
        << "  scores       Show scores and status\n"
        << "  depth N      Set AI search depth (default 5)\n"
        << "  time N       Set AI time limit in ms (default 5000)\n"
        << "  resign       Current human player resigns\n"
        << "  quit / exit  Quit the game\n"
        << "  help         Show this message\n\n"
        << "Piece colours on board:\n"
        << Ansi::pieceBg(RED)   << " R " << Ansi::RESET << " Red   (AI)   — right side, moves LEFT\n"
        << Ansi::pieceBg(BLACK) << " B " << Ansi::RESET << " Black (Human)— top,   moves DOWN\n"
        << Ansi::pieceBg(GREEN) << " G " << Ansi::RESET << " Green (Human)— left,  moves RIGHT\n"
        << Ansi::pieceBg(BLUE)  << " B " << Ansi::RESET << " Blue  (Human)— bottom,moves UP\n\n"
        << "Scoring: P=1pt  N/B=3pt  R=5pt  Q=9pt  Checkmate=+20pt\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Check if game is over
// ─────────────────────────────────────────────────────────────────────────────
static bool gameOver(Board& b){
    int alive=0;
    for(int c=1;c<=4;c++)
        if(!b.ps[c].eliminated) alive++;
    return alive<=1;
}

static Color findWinner(const Board& b){
    // Last player standing, or highest score
    for(int c=1;c<=4;c++)
        if(!b.ps[c].eliminated) return (Color)c;
    // All eliminated — find highest score
    Color best=RED; int bestScore=-1;
    for(int c=1;c<=4;c++)
        if(b.ps[c].score>bestScore){ bestScore=b.ps[c].score; best=(Color)c; }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apply a human's move — validate it, update state, handle captures/checkmate
// ─────────────────────────────────────────────────────────────────────────────
static bool applyHumanMove(Board& b, const Move& m){
    Color col = b.currentPlayer();

    // Verify the move is legal
    std::vector<Move> legal;
    b.legalMoves(col, legal);
    bool found=false;
    Move chosen{};
    for(auto& lm:legal){
        if(lm.sr==m.sr && lm.sc==m.sc && lm.tr==m.tr && lm.tc==m.tc){
            chosen=lm; found=true; break;
        }
    }
    if(!found){
        std::cout << Ansi::YELLOW_FG << "Illegal move. Type 'moves' to see legal moves.\n" << Ansi::RESET;
        return false;
    }

    // Record capture
    Piece cap = b.at(m.tr,m.tc);
    std::cout << "\n" << Ansi::BOLD << colorName(col) << Ansi::RESET
              << ": " << pieceChar(b.at(m.sr,m.sc).type)
              << " (" << (int)m.sr << "," << (int)m.sc << ") → ("
              << (int)m.tr << "," << (int)m.tc << ")";
    if(!cap.empty()){
        int pts = PieceVal::pts[(int)cap.type];
        b.ps[col].score += pts;
        std::cout << "  captures " << colorName(cap.color) << " "
                  << pieceChar(cap.type) << " +" << pts << "pt";
    }
    std::cout << "\n";

    b.applyMove(chosen);

    // Check for checkmates resulting from this move
    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==col || b.ps[opp].eliminated) continue;
        if(b.isCheckmated(opp)){
            b.ps[opp].eliminated=true;
            b.ps[col].score+=20;
            std::cout << Ansi::YELLOW_FG << "💀 " << colorName(opp)
                      << " is CHECKMATED! " << colorName(col)
                      << " earns +20 points!\n" << Ansi::RESET;
        } else if(b.isInCheck(opp)){
            std::cout << Ansi::MAGENTA_FG << "⚠  " << colorName(opp)
                      << " is in CHECK!\n" << Ansi::RESET;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AI turn
// ─────────────────────────────────────────────────────────────────────────────
static void doAiMove(Board& b, Engine& eng){
    std::cout << "\n" << Ansi::pieceBg(RED) << " AI (Red) is thinking... " << Ansi::RESET << "\n";
    std::cout.flush();

    SearchResult res = eng.search(b);

    if(!res.bestMove.valid()){
        std::cout << Ansi::YELLOW_FG << "AI has no valid move.\n" << Ansi::RESET;
        // Advance turn manually
        b.turnIdx=(b.turnIdx+1)%4;
        return;
    }

    Piece mover = b.at(res.bestMove.sr, res.bestMove.sc);
    Piece cap   = b.at(res.bestMove.tr, res.bestMove.tc);

    std::cout << Ansi::pieceBg(RED) << " AI " << Ansi::RESET
              << " plays: " << Ansi::BOLD
              << pieceChar(mover.type)
              << " (" << (int)res.bestMove.sr << "," << (int)res.bestMove.sc << ") → ("
              << (int)res.bestMove.tr << "," << (int)res.bestMove.tc << ")"
              << Ansi::RESET;
    if(!cap.empty()){
        int pts=PieceVal::pts[(int)cap.type];
        b.ps[RED].score+=pts;
        std::cout << "  captures " << colorName(cap.color) << " "
                  << pieceChar(cap.type) << " +" << pts << "pt";
    }
    if(res.bestMove.castleKingside)  std::cout << " (O-O)";
    if(res.bestMove.castleQueenside) std::cout << " (O-O-O)";
    std::cout << "  [depth=" << res.depth << " score=" << res.score
              << " nodes=" << res.nodes << "]\n";

    b.applyMove(res.bestMove);

    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==RED || b.ps[opp].eliminated) continue;
        if(b.isCheckmated(opp)){
            b.ps[opp].eliminated=true;
            b.ps[RED].score+=20;
            std::cout << Ansi::YELLOW_FG << "💀 " << colorName(opp)
                      << " is CHECKMATED by AI! +20 points!\n" << Ansi::RESET;
        } else if(b.isInCheck(opp)){
            std::cout << Ansi::MAGENTA_FG << "⚠  " << colorName(opp)
                      << " is in CHECK!\n" << Ansi::RESET;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main game loop
// ─────────────────────────────────────────────────────────────────────────────
void Game::start(){
    Zobrist::init();
    std::fill(std::begin(g_tt), std::end(g_tt), TTEntry{});

    engine.aiColor   = RED;
    engine.maxDepth  = 5;
    engine.timeLimit_ms = 5000;

    board.reset();

    std::cout << "\n"
        << Ansi::BOLD << Ansi::CYAN_FG
        << "╔═══════════════════════════════════════════════╗\n"
        << "║      4-Player FFA Chess Engine  v1.0          ║\n"
        << "║   AI plays Red  │  You play Black/Green/Blue  ║\n"
        << "╚═══════════════════════════════════════════════╝\n"
        << Ansi::RESET;

    printHelp();
    printBoard();
    printScores();

    std::string line;
    while(true){
        if(gameOver(board)){
            Color winner = findWinner(board);
            std::cout << "\n" << Ansi::BOLD << Ansi::YELLOW_FG
                      << "🏆  GAME OVER! Winner: " << colorName(winner)
                      << " with " << board.ps[winner].score << " points!\n"
                      << Ansi::RESET;
            printScores();
            break;
        }

        Color cur = board.currentPlayer();
        if(cur==NO_COLOR) break;

        // ── AI turn
        if(cur==RED){
            doAiMove(board, engine);
            printBoard();
            printScores();
            continue;
        }

        // ── Human turn
        std::cout << Ansi::playerColor(cur) << Ansi::BOLD
                  << "\n▶  " << colorName(cur) << "'s turn"
                  << Ansi::RESET << "  (enter move or 'help'): ";
        std::cout.flush();

        if(!std::getline(std::cin, line)){
            // EOF
            break;
        }

        // Trim
        while(!line.empty()&&std::isspace((unsigned char)line.front())) line.erase(line.begin());
        while(!line.empty()&&std::isspace((unsigned char)line.back()))  line.pop_back();
        if(line.empty()) continue;

        // ── Commands
        if(line=="quit"||line=="exit"){
            std::cout << "Goodbye!\n";
            break;
        }
        if(line=="help")   { printHelp();       continue; }
        if(line=="board")  { printBoard();       continue; }
        if(line=="scores") { printScores();      continue; }
        if(line=="moves")  { printLegalMoves(cur); continue; }

        if(line=="resign"){
            board.ps[cur].eliminated=true;
            std::cout << Ansi::YELLOW_FG << colorName(cur) << " has resigned.\n" << Ansi::RESET;
            board.turnIdx=(board.turnIdx+1)%4;
            printBoard(); printScores();
            continue;
        }

        // depth / time commands
        if(line.rfind("depth",0)==0){
            try{
                engine.maxDepth=std::stoi(line.substr(5));
                std::cout << "AI depth set to " << engine.maxDepth << "\n";
            } catch(...){ std::cout << "Usage: depth N\n"; }
            continue;
        }
        if(line.rfind("time",0)==0){
            try{
                engine.timeLimit_ms=std::stoi(line.substr(4));
                std::cout << "AI time limit set to " << engine.timeLimit_ms << "ms\n";
            } catch(...){ std::cout << "Usage: time N\n"; }
            continue;
        }

        // ── Parse as move
        auto mv = strToMove(line, cur);
        if(!mv){
            std::cout << Ansi::YELLOW_FG
                      << "Unknown command or invalid move format.\n"
                      << "Type 'help' for usage, 'moves' for legal moves.\n"
                      << Ansi::RESET;
            continue;
        }

        if(applyHumanMove(board, *mv)){
            printBoard();
            printScores();
        }
    }
}
