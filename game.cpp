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

static std::string colorNameLower(Color c){
    std::string s = colorName(c);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char cc){ return (unsigned char)std::tolower(cc); });
    return s;
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
    oss << fc << (int)(m.sr+1) << tc << (int)(m.tr+1);
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
        size_t p2=1; while(p2<tok.size()&&std::isdigit(tok[p2])) p2++;
        if(p2<2 || p2>=tok.size()) return std::nullopt;
        int sr2 = std::stoi(tok.substr(1, p2-1)) - 1;

        int tc2 = std::tolower(tok[p2])-'a';
        int tr2 = std::stoi(tok.substr(p2+1)) - 1;

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
    for(int c=1;c<=4;c++)
        if(!b.ps[c].eliminated) return (Color)c;
    Color best=RED; int bestScore=-1;
    for(int c=1;c<=4;c++)
        if(b.ps[c].score>bestScore){ bestScore=b.ps[c].score; best=(Color)c; }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AI turn
// ─────────────────────────────────────────────────────────────────────────────
static void doAiMove(Board& b, Engine& eng, bool proto){
    if(!proto){
        std::cout << "\n" << Ansi::pieceBg(RED) << " AI (Red) is thinking... " << Ansi::RESET << "\n";
        std::cout.flush();
    }

    SearchResult res = eng.search(b);

    if(!res.bestMove.valid()){
        if(!proto) std::cout << Ansi::YELLOW_FG << "AI has no valid move.\n" << Ansi::RESET;
        else std::cout << "bestmove none\n";
        b.turnIdx=(b.turnIdx+1)%4;
        return;
    }

    Piece mover = b.at(res.bestMove.sr, res.bestMove.sc);
    Piece cap   = b.at(res.bestMove.tr, res.bestMove.tc);

    if(proto){
        std::cout << "bestmove ";
        char fc = 'a' + res.bestMove.sc;
        char tc = 'a' + res.bestMove.tc;
        std::cout << fc << (int)(res.bestMove.sr+1) << tc << (int)(res.bestMove.tr+1);
        if(res.bestMove.promotion != NONE) std::cout << (char)std::tolower(pieceChar(res.bestMove.promotion));
        std::cout << "\n";
    } else {
        std::cout << Ansi::pieceBg(RED) << " AI " << Ansi::RESET
                  << " plays: " << Ansi::BOLD
                  << pieceChar(mover.type)
                  << " (" << (int)res.bestMove.sr << "," << (int)res.bestMove.sc << ") → ("
                  << (int)res.bestMove.tr << "," << (int)res.bestMove.tc << ")"
                  << Ansi::RESET;
        if(!cap.empty()){
            int pts=PieceVal::pts[(int)cap.type];
            std::cout << "  captures " << colorName(cap.color) << " "
                      << pieceChar(cap.type) << " +" << pts << "pt";
        }
        if(res.bestMove.castleKingside)  std::cout << " (O-O)";
        if(res.bestMove.castleQueenside) std::cout << " (O-O-O)";
        std::cout << "  [depth=" << res.depth << " score=" << res.score
                  << " nodes=" << res.nodes << "]\n";
    }

    if(!cap.empty()){
        int pts=PieceVal::pts[(int)cap.type];
        b.ps[RED].score+=pts;
    }

    b.applyMove(res.bestMove);

    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==RED || b.ps[opp].eliminated) continue;
        if(b.isCheckmated(opp)){
            b.ps[opp].eliminated=true;
            b.ps[RED].score+=20;
            if(!proto)
                std::cout << Ansi::YELLOW_FG << "💀 " << colorName(opp)
                          << " is CHECKMATED by AI! +20 points!\n" << Ansi::RESET;
        } else if(b.isInCheck(opp)){
            if(!proto)
                std::cout << Ansi::MAGENTA_FG << "⚠  " << colorName(opp)
                          << " is in CHECK!\n" << Ansi::RESET;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apply a human's move
// ─────────────────────────────────────────────────────────────────────────────
static bool applyHumanMove(Board& b, const Move& m, bool proto){
    Color col = b.currentPlayer();
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
        if(!proto) std::cout << Ansi::YELLOW_FG << "Illegal move. Type 'moves' to see legal moves.\n" << Ansi::RESET;
        else std::cout << "illegal\n";
        return false;
    }

    Piece cap = b.at(m.tr,m.tc);
    if(!proto){
        std::cout << "\n" << Ansi::BOLD << colorName(col) << Ansi::RESET
                  << ": " << pieceChar(b.at(m.sr,m.sc).type)
                  << " (" << (int)m.sr << "," << (int)m.sc << ") → ("
                  << (int)m.tr << "," << (int)m.tc << ")";
        if(!cap.empty()){
            int pts = PieceVal::pts[(int)cap.type];
            std::cout << "  captures " << colorName(cap.color) << " "
                      << pieceChar(cap.type) << " +" << pts << "pt";
        }
        std::cout << "\n";
    }
    if(!cap.empty()){
        int pts = PieceVal::pts[(int)cap.type];
        b.ps[col].score += pts;
    }

    b.applyMove(chosen);

    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==col || b.ps[opp].eliminated) continue;
        if(b.isCheckmated(opp)){
            b.ps[opp].eliminated=true;
            b.ps[col].score+=20;
            if(!proto)
                std::cout << Ansi::YELLOW_FG << "💀 " << colorName(opp)
                          << " is CHECKMATED! " << colorName(col)
                          << " earns +20 points!\n" << Ansi::RESET;
        } else if(b.isInCheck(opp)){
            if(!proto)
                std::cout << Ansi::MAGENTA_FG << "⚠  " << colorName(opp)
                          << " is in CHECK!\n" << Ansi::RESET;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main game loop
// ─────────────────────────────────────────────────────────────────────────────
void Game::start(bool proto){
    protoMode = proto;
    Zobrist::init();
    std::fill(std::begin(g_tt), std::end(g_tt), TTEntry{});

    engine.aiColor   = RED;
    engine.maxDepth  = 5;
    engine.timeLimit_ms = 5000;

    board.reset();

    if(!proto){
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
    }

    std::string line;
    while(true){
        if(gameOver(board)){
            if(!proto){
                Color winner = findWinner(board);
                std::cout << "\n" << Ansi::BOLD << Ansi::YELLOW_FG
                          << "🏆  GAME OVER! Winner: " << colorName(winner)
                          << " with " << board.ps[winner].score << " points!\n"
                          << Ansi::RESET;
                printScores();
            } else {
                Color winner = findWinner(board);
                std::cout << "gameover " << colorNameLower(winner) << "\n";
            }
            break;
        }

        if(board.isDraw()){
            if(!proto) std::cout << "\n" << Ansi::BOLD << Ansi::YELLOW_FG << "⚖  DRAW!\n" << Ansi::RESET;
            else std::cout << "draw\n";
            break;
        }

        Color cur = board.currentPlayer();
        if(cur==NO_COLOR) break;

        if(proto) {
            std::cout << "turn " << colorNameLower(cur) << "\n";
        }

        if(!proto && cur==RED){
            doAiMove(board, engine, proto);
            printBoard();
            printScores();
            continue;
        }

        if(!proto){
            std::cout << Ansi::playerColor(cur) << Ansi::BOLD
                      << "\n▶  " << colorName(cur) << "'s turn"
                      << Ansi::RESET << "  (enter move or 'help'): ";
            std::cout.flush();
        }

        if(!std::getline(std::cin, line)){
            break;
        }

        while(!line.empty()&&std::isspace((unsigned char)line.front())) line.erase(line.begin());
        while(!line.empty()&&std::isspace((unsigned char)line.back()))  line.pop_back();
        if(line.empty()) continue;

        if(line=="quit"||line=="exit"){
            if(!proto) std::cout << "Goodbye!\n";
            break;
        }
        if(proto && line == "proto") { std::cout << "protook\n"; continue; }
        if(proto && line == "ready") { std::cout << "readyok\n"; continue; }
        if(proto && line == "go") { doAiMove(board, engine, proto); continue; }
        if(proto && line == "board") {
            std::cout << "board\n";
            for(int r=0; r<ROWS; r++){
                for(int c=0; c<COLS; c++){
                    if(!inBounds(r,c)) continue;
                    Piece p = board.at(r,c);
                    if(!p.empty()){
                        std::cout << r << " " << c << " "
                                  << colorNameLower(p.color) << " "
                                  << pieceChar(p.type) << "\n";
                    }
                }
            }
            std::cout << "boardok\n";
            continue;
        }
        if(proto && line.rfind("perft ", 0) == 0) {
            int d = std::stoi(line.substr(6));
            auto start = std::chrono::steady_clock::now();
            auto res = board.perft(d);
            auto end = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
            std::cout << "perft depth " << d << " nodes " << res.nodes << " time " << ms << "ms\n";
            continue;
        }
        if(proto && line.rfind("position ", 0) == 0) {
            board.reset();
            size_t movesPos = line.find("moves ");
            if(movesPos != std::string::npos) {
                std::string movesStr = line.substr(movesPos + 6);
                std::istringstream iss(movesStr);
                std::string mvStr;
                while(iss >> mvStr) {
                    auto mv = strToMove(mvStr, board.currentPlayer());
                    if(mv) {
                        std::vector<Move> legal;
                        board.legalMoves(board.currentPlayer(), legal);
                        for(auto& lm : legal) {
                            if(lm.sr == mv->sr && lm.sc == mv->sc && lm.tr == mv->tr && lm.tc == mv->tc) {
                                board.applyMove(lm);
                                break;
                            }
                        }
                    }
                }
            }
            continue;
        }
        if(line=="help")   { printHelp();       continue; }
        if(line=="board")  { printBoard();       continue; }
        if(line=="scores") { printScores();      continue; }
        if(line=="moves")  { printLegalMoves(cur); continue; }

        if(line=="resign"){
            board.ps[cur].eliminated=true;
            if(!proto) {
                std::cout << Ansi::YELLOW_FG << colorName(cur) << " has resigned.\n" << Ansi::RESET;
                board.turnIdx=(board.turnIdx+1)%4;
                printBoard(); printScores();
            } else {
                board.turnIdx=(board.turnIdx+1)%4;
                std::cout << "moveok\n";
            }
            continue;
        }

        if(line.rfind("depth",0)==0){
            try{
                engine.maxDepth=std::stoi(line.substr(5));
                if(!proto) std::cout << "AI depth set to " << engine.maxDepth << "\n";
            } catch(...){ if(!proto) std::cout << "Usage: depth N\n"; }
            continue;
        }
        if(line.rfind("time",0)==0){
            try{
                engine.timeLimit_ms=std::stoi(line.substr(4));
                if(!proto) std::cout << "AI time limit set to " << engine.timeLimit_ms << "ms\n";
            } catch(...){ if(!proto) std::cout << "Usage: time N\n"; }
            continue;
        }

        auto mv = strToMove(line, cur);
        if(!mv){
            if(!proto) std::cout << Ansi::YELLOW_FG << "Unknown command or invalid move format.\n" << Ansi::RESET;
            else std::cout << "illegal\n";
            continue;
        }

        if(applyHumanMove(board, *mv, proto)){
            if(!proto) {
                printBoard();
                printScores();
            } else {
                std::cout << "moveok\n";
                if(board.currentPlayer() == RED){
                    doAiMove(board, engine, proto);
                }
            }
        }
    }
}
