#include "chess4.hpp"
#include <cstring>
#include <random>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Zobrist
// ─────────────────────────────────────────────────────────────────────────────
namespace Zobrist {
    uint64_t table[5][7][ROWS][COLS];
    uint64_t sideToMove[4];
    uint64_t castle[4][2];
    uint64_t enPassant[COLS];
    void init(){
        std::mt19937_64 rng(0xDEADBEEFCAFEBABEULL);
        for(int c=1;c<=4;c++)
            for(int t=1;t<=6;t++)
                for(int r=0;r<ROWS;r++)
                    for(int cc=0;cc<COLS;cc++)
                        table[c][t][r][cc] = rng();
        for(int i=0;i<4;i++) sideToMove[i]=rng();
        for(int i=0;i<4;i++) for(int j=0;j<2;j++) castle[i][j]=rng();
        for(int i=0;i<COLS;i++) enPassant[i]=rng();
    }
}

TTEntry g_tt[TT_SIZE];

// ─────────────────────────────────────────────────────────────────────────────
//  Board helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Sets a piece at (r,c) and updates the Zobrist hash incrementally.
 * @param r Row index
 * @param c Column index
 * @param p Piece to place
 */
void Board::set(int r, int c, Piece p){
    if(!inBounds(r,c)) return;
    Piece old = cells[r][c];
    if(!old.empty())
        hash ^= Zobrist::table[old.color][old.type][r][c];
    cells[r][c] = p;
    if(!p.empty())
        hash ^= Zobrist::table[p.color][p.type][r][c];
}

static const PieceType BACK_ROW[8] = { R,N,B,Q,K,B,N,R };

void Board::reset(){
    memset(cells, 0, sizeof(cells));
    for(int i=1;i<=4;i++) ps[i] = PlayerState{};
    turnIdx = 0;
    enPassantSq = {-1, -1};
    halfmoveClock = 0;
    history.clear();
    turnOrder[0]=RED; turnOrder[1]=BLACK; turnOrder[2]=GREEN; turnOrder[3]=BLUE;

    hash = 0;
    hash ^= Zobrist::sideToMove[turnIdx];

    // Black at top (rows 0-1, cols 3-10), moves DOWN
    for(int i=0;i<8;i++){
        set(0, 3+i, {BACK_ROW[i], BLACK});
        set(1, 3+i, {P,           BLACK});
    }
    // Blue at bottom (rows 12-13, cols 3-10), moves UP
    for(int i=0;i<8;i++){
        set(13, 3+i, {BACK_ROW[i], BLUE});
        set(12, 3+i, {P,           BLUE});
    }
    // Green at left (rows 3-10, cols 0-1), moves RIGHT
    for(int i=0;i<8;i++){
        set(3+i, 0, {BACK_ROW[i], GREEN});
        set(3+i, 1, {P,           GREEN});
    }
    // Red at right (rows 3-10, cols 12-13), moves LEFT
    for(int i=0;i<8;i++){
        set(3+i, 13, {BACK_ROW[i], RED});
        set(3+i, 12, {P,           RED});
    }
}

Sq Board::findKing(Color col) const {
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++){
            auto& p=cells[r][c];
            if(p.type==K && p.color==col) return {r,c};
        }
    return {-1,-1};
}

// ─────────────────────────────────────────────────────────────────────────────
//  Move generation (pseudo-legal)
// ─────────────────────────────────────────────────────────────────────────────
static inline bool isEnemy(Piece p, Color col){ return !p.empty() && p.color!=col; }
static inline bool isFriend(Piece p, Color col){ return !p.empty() && p.color==col; }

static bool isPawnStart(int r, int c, Color col){
    switch(col){
        case BLACK: return r==1 && c>=3 && c<=10;
        case BLUE:  return r==12 && c>=3 && c<=10;
        case GREEN: return c==1 && r>=3 && r<=10;
        case RED:   return c==12 && r>=3 && r<=10;
        default:    return false;
    }
}

// Returns the promotion rank for a pawn (returns true if it should promote)
static bool shouldPromote(int tr, int tc, Color col){
    switch(col){
        case BLACK: return tr==7;   // reaches middle row going down
        case BLUE:  return tr==6;   // reaches middle row going up
        case GREEN: return tc==7;   // reaches middle col going right
        case RED:   return tc==6;   // reaches middle col going left
        default:    return false;
    }
}

// Pawn forward direction per color
static void pawnDelta(Color col, int& dr, int& dc){
    switch(col){
        case BLACK: dr=1;  dc=0;  break;
        case BLUE:  dr=-1; dc=0;  break;
        case GREEN: dr=0;  dc=1;  break;
        case RED:   dr=0;  dc=-1; break;
        default:    dr=0;  dc=0;  break;
    }
}
// Pawn capture diagonals (perpendicular to forward)
void Board::pawnCapDeltas(Color col, int ds[2][2]){
    switch(col){
        case BLACK: ds[0][0]=1; ds[0][1]=-1; ds[1][0]=1; ds[1][1]=1; break;
        case BLUE:  ds[0][0]=-1; ds[0][1]=-1; ds[1][0]=-1; ds[1][1]=1; break;
        case GREEN: ds[0][0]=-1; ds[0][1]=1; ds[1][0]=1; ds[1][1]=1; break;
        case RED:   ds[0][0]=-1; ds[0][1]=-1; ds[1][0]=1; ds[1][1]=-1; break;
        default:    break;
    }
}

void Board::genMovesFor(int r, int c, std::vector<Move>& out) const {
    Piece p = cells[r][c];
    if(p.empty()) return;
    Color col = p.color;

    auto tryPush = [&](int nr, int nc, PieceType promo=NONE){
        if(!inBounds(nr,nc)) return;
        Piece t = cells[nr][nc];
        if(isFriend(t, col)) return;
        Move m; m.sr=r; m.sc=c; m.tr=nr; m.tc=nc; m.promotion=promo;
        out.push_back(m);
    };

    auto slidePush = [&](int dr, int dc){
        int nr=r+dr, nc=c+dc;
        while(inBounds(nr,nc)){
            Piece t = cells[nr][nc];
            if(t.empty()){
                Move m; m.sr=r; m.sc=c; m.tr=nr; m.tc=nc;
                out.push_back(m);
            } else {
                if(isEnemy(t,col)){ Move m; m.sr=r; m.sc=c; m.tr=nr; m.tc=nc; out.push_back(m); }
                break;
            }
            nr+=dr; nc+=dc;
        }
    };

    switch(p.type){
        case P: {
            int dr,dc; pawnDelta(col,dr,dc);
            int nr=r+dr, nc=c+dc;
            if(inBounds(nr,nc) && cells[nr][nc].empty()){
                PieceType promo = shouldPromote(nr,nc,col) ? Q : NONE;
                tryPush(nr,nc,promo);
                // double push from start
                if(!promo && isPawnStart(r,c,col)){
                    int nr2=r+2*dr, nc2=c+2*dc;
                    if(inBounds(nr2,nc2) && cells[nr2][nc2].empty())
                        tryPush(nr2,nc2);
                }
            }
            // captures
            int ds[2][2]; pawnCapDeltas(col,ds);
            for(int i=0;i<2;i++){
                int ar=r+ds[i][0], ac=c+ds[i][1];
                if(inBounds(ar,ac)){
                    if(isEnemy(cells[ar][ac],col)){
                        PieceType promo = shouldPromote(ar,ac,col) ? Q : NONE;
                        tryPush(ar,ac,promo);
                    } else if(ar==enPassantSq.r && ac==enPassantSq.c){
                        Move em; em.sr=r; em.sc=c; em.tr=ar; em.tc=ac; em.isEnPassant=true;
                        out.push_back(em);
                    }
                }
            }
            break;
        }
        case N: {
            static const int kd[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
            for(auto& d:kd) tryPush(r+d[0],c+d[1]);
            break;
        }
        case B:
            slidePush(1,1); slidePush(1,-1); slidePush(-1,1); slidePush(-1,-1);
            break;
        case R:
            slidePush(1,0); slidePush(-1,0); slidePush(0,1); slidePush(0,-1);
            // Castling
            if(!ps[col].kingMoved && !ps[col].castledK && !ps[col].castledQ){
                // Basic castling rights check
                // Kingside
                if(!ps[col].rookKMoved){
                    // Will be handled in King generation
                }
            }
            break;
        case Q:
            slidePush(1,0); slidePush(-1,0); slidePush(0,1); slidePush(0,-1);
            slidePush(1,1); slidePush(1,-1); slidePush(-1,1); slidePush(-1,-1);
            break;
        case K: {
            for(int dr=-1;dr<=1;dr++)
                for(int dc=-1;dc<=1;dc++)
                    if(dr||dc) tryPush(r+dr,c+dc);
            // Castling (simplified — check empty squares between king and rook)
            if(!ps[col].kingMoved){
                // Each player's king and rook positions at start
                // Red:   King at (6,13) or (7,13), Rooks at (3,13) and (10,13)
                // Black: King at (0,7), Rooks at (0,3) and (0,10)
                // Blue:  King at (13,7), Rooks at (13,3) and (13,10)
                // Green: King at (7,0), Rooks at (3,0) and (10,0)
                struct CastleInfo { int kr,kc,rr,rc,dr2,dc2,steps; };
                std::vector<CastleInfo> options;
                if(col==BLACK){
                    options.push_back({0,7,0,3, 0,-1,3}); // queenside
                    options.push_back({0,7,0,10,0, 1,2}); // kingside
                } else if(col==BLUE){
                    options.push_back({13,7,13,3, 0,-1,3});
                    options.push_back({13,7,13,10,0, 1,2});
                } else if(col==GREEN){
                    options.push_back({7,0,3,0, -1,0,3});
                    options.push_back({7,0,10,0, 1,0,2});
                } else { // RED
                    options.push_back({6,13,3,13,-1,0,2});
                    options.push_back({6,13,10,13,1,0,3});
                }
                for(auto& ci:options){
                    if(r!=ci.kr || c!=ci.kc) continue;
                    Piece rook = cells[ci.rr][ci.rc];
                    if(rook.type!=::R || rook.color!=col) continue;
                    // Check squares between are empty
                    bool clear=true;
                    int nr=ci.kr+ci.dr2, nc=ci.kc+ci.dc2;
                    for(int s=0;s<ci.steps;s++){
                        if(!cells[nr][nc].empty()){clear=false;break;}
                        nr+=ci.dr2; nc+=ci.dc2;
                    }
                    if(!clear) continue;
                    // King moves 2 squares toward rook
                    Move cm; cm.sr=r; cm.sc=c;
                    cm.tr=r+ci.dr2*2; cm.tc=c+ci.dc2*2;
                    if(ci.dr2<0||ci.dc2<0) cm.castleQueenside=true;
                    else cm.castleKingside=true;
                    if(inBounds(cm.tr,cm.tc)) out.push_back(cm);
                }
            }
            break;
        }
        default: break;
    }
}

void Board::genAllMoves(Color col, std::vector<Move>& out) const {
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++)
            if(cells[r][c].color==col)
                genMovesFor(r,c,out);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Check detection
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Efficiently determines if a square is attacked by a specific player.
 * Avoids full move generation by scanning outwards from the target square.
 * @param r Target row
 * @param c Target column
 * @param attacker Color of the potential attacking player
 * @return true if the square is attacked
 */
bool Board::isAttacked(int r, int c, Color attacker) const {
    // Knight
    static const int kn[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for(auto& d:kn){
        int nr=r+d[0], nc=c+d[1];
        if(inBounds(nr,nc)){
            Piece p = cells[nr][nc];
            if(p.type==N && p.color==attacker) return true;
        }
    }
    // King
    for(int dr=-1;dr<=1;dr++)
        for(int dc=-1;dc<=1;dc++)
            if(dr||dc){
                int nr=r+dr, nc=c+dc;
                if(inBounds(nr,nc)){
                    Piece p = cells[nr][nc];
                    if(p.type==K && p.color==attacker) return true;
                }
            }
    // Sliding pieces
    auto slide = [&](int dr, int dc, PieceType t1, PieceType t2){
        int nr=r+dr, nc=c+dc;
        while(inBounds(nr,nc)){
            Piece p = cells[nr][nc];
            if(!p.empty()){
                if(p.color==attacker && (p.type==t1 || p.type==t2 || p.type==Q)) return true;
                break;
            }
            nr+=dr; nc+=dc;
        }
        return false;
    };
    if(slide(1,0, R, R) || slide(-1,0, R, R) || slide(0,1, R, R) || slide(0,-1, R, R)) return true;
    if(slide(1,1, B, B) || slide(1,-1, B, B) || slide(-1,1, B, B) || slide(-1,-1, B, B)) return true;

    // Pawns: if 'attacker' has a pawn at (pr,pc) that can capture (r,c)
    int ds[2][2];
    Board::pawnCapDeltas(attacker, ds);
    for(int i=0;i<2;i++){
        int pr = r - ds[i][0];
        int pc = c - ds[i][1];
        if(inBounds(pr,pc)){
            Piece p = cells[pr][pc];
            if(p.type==P && p.color==attacker) return true;
        }
    }
    return false;
}

bool Board::isInCheck(Color col) const {
    Sq king = findKing(col);
    if(king.r<0) return true; // no king = in check
    // Check if any other active player attacks the king
    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==col || ps[opp].eliminated) continue;
        if(isAttacked(king.r, king.c, opp)) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apply / undo move
// ─────────────────────────────────────────────────────────────────────────────
void Board::applyMove(const Move& mv){
    Piece mover = cells[mv.sr][mv.sc];
    Color col = mover.color;

    // Track king/rook movement for castling rights
    if(mover.type==K){ ps[col].kingMoved=true; }
    if(mover.type==R){
        // Determine if kingside or queenside rook
        if(col==BLACK){
            if(mv.sc==10) ps[col].rookKMoved=true;
            if(mv.sc==3)  ps[col].rookQMoved=true;
        } else if(col==BLUE){
            if(mv.sc==10) ps[col].rookKMoved=true;
            if(mv.sc==3)  ps[col].rookQMoved=true;
        } else if(col==GREEN){
            if(mv.sr==10) ps[col].rookKMoved=true;
            if(mv.sr==3)  ps[col].rookQMoved=true;
        } else {
            if(mv.sr==10) ps[col].rookKMoved=true;
            if(mv.sr==3)  ps[col].rookQMoved=true;
        }
    }

    // Handle castling: move rook too
    if(mv.castleKingside||mv.castleQueenside){
        // Find rook position
        int rr,rc,nrr,nrc;
        if(col==BLACK){
            rr=0; rc=mv.castleKingside?10:3;
            nrr=0; nrc=mv.castleKingside?mv.tc-1:mv.tc+1;
        } else if(col==BLUE){
            rr=13; rc=mv.castleKingside?10:3;
            nrr=13; nrc=mv.castleKingside?mv.tc-1:mv.tc+1;
        } else if(col==GREEN){
            rc=0; rr=mv.castleKingside?10:3;
            nrc=0; nrr=mv.castleKingside?mv.tr-1:mv.tr+1;
        } else {
            rc=13; rr=mv.castleKingside?10:3;
            nrc=13; nrr=mv.castleKingside?mv.tr-1:mv.tr+1;
        }
        Piece rook = cells[rr][rc];
        set(rr,rc,NO_PIECE);
        if(inBounds(nrr,nrc)) set(nrr,nrc,rook);
    }

    Piece victim = cells[mv.tr][mv.tc];

    // Update halfmove clock: reset on pawn move or capture
    if(mover.type==P || !victim.empty()) halfmoveClock = 0;
    else halfmoveClock++;

    set(mv.sr, mv.sc, NO_PIECE);
    Piece landing = mv.promotion!=NONE ? Piece{mv.promotion, col} : mover;
    set(mv.tr, mv.tc, landing);

    // Handle En Passant capture
    if(mv.isEnPassant){
        int dr, dc;
        pawnDelta(col, dr, dc);
        // The captured pawn is one step BEHIND the target square in the perspective of the mover
        set(mv.tr - dr, mv.tc - dc, NO_PIECE);
    }

    // Set new En Passant square
    if(enPassantSq.r != -1) hash ^= Zobrist::enPassant[enPassantSq.c];
    enPassantSq = {-1, -1};
    if(mover.type==P && std::abs(mv.tr-mv.sr)+std::abs(mv.tc-mv.sc)==2){
        // Double push
        int dr, dc;
        pawnDelta(col, dr, dc);
        enPassantSq = {mv.sr + dr, mv.sc + dc};
        hash ^= Zobrist::enPassant[enPassantSq.c];
    }

    // Record history for repetition
    history.push_back(hash);

    // Advance turn index
    hash ^= Zobrist::sideToMove[turnIdx];
    int next=(turnIdx+1)%4;
    while(ps[turnOrder[next]].eliminated && next!=turnIdx) next=(next+1)%4;
    turnIdx=next;
    hash ^= Zobrist::sideToMove[turnIdx];
}

void Board::undoMove(const Move& /*mv*/, Board& saved){
    *this = saved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Legal moves (filter pseudo-legal: don't leave own king in check)
// ─────────────────────────────────────────────────────────────────────────────
void Board::legalMoves(Color col, std::vector<Move>& out){
    std::vector<Move> pseudo;
    pseudo.reserve(64);
    genAllMoves(col, pseudo);
    out.reserve(pseudo.size());
    for(auto& m:pseudo){
        Board copy = clone();
        // Temporarily apply
        Piece cap = copy.cells[m.tr][m.tc];
        copy.cells[m.tr][m.tc] = copy.cells[m.sr][m.sc];
        if(m.promotion!=NONE) copy.cells[m.tr][m.tc].type = m.promotion;
        copy.cells[m.sr][m.sc] = NO_PIECE;
        // Handle castling rook
        if(m.castleKingside||m.castleQueenside){
            int rr,rc,nrr,nrc;
            if(col==BLACK){ rr=0;rc=m.castleKingside?10:3; nrr=0;nrc=m.castleKingside?m.tc-1:m.tc+1; }
            else if(col==BLUE){ rr=13;rc=m.castleKingside?10:3; nrr=13;nrc=m.castleKingside?m.tc-1:m.tc+1; }
            else if(col==GREEN){ rc=0;rr=m.castleKingside?10:3; nrc=0;nrr=m.castleKingside?m.tr-1:m.tr+1; }
            else { rc=13;rr=m.castleKingside?10:3; nrc=13;nrr=m.castleKingside?m.tr-1:m.tr+1; }
            Piece rook=copy.cells[rr][rc];
            copy.cells[rr][rc]=NO_PIECE;
            if(inBounds(nrr,nrc)) copy.cells[nrr][nrc]=rook;
            (void)cap;
        }
        if(!copy.isInCheck(col)) out.push_back(m);
    }
}

bool Board::isCheckmated(Color col){
    if(!isInCheck(col)) return false;
    std::vector<Move> legal;
    legalMoves(col, legal);
    return legal.empty();
}

bool Board::isDraw() const {
    if(halfmoveClock >= 100) return true; // 50-move rule
    // 3-fold repetition
    int count = 0;
    for(uint64_t h : history){
        if(h == hash) count++;
    }
    return count >= 3;
}

bool Board::isStalemate(Color col){
    if(isInCheck(col)) return false;
    std::vector<Move> legal;
    legalMoves(col, legal);
    return legal.empty();
}

int Board::materialOf(Color col) const {
    int total=0;
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++){
            auto& p=cells[r][c];
            if(p.color==col) total+=PieceVal::get(p.type);
        }
    return total;
}

int Board::pieceCount(Color col) const {
    int n=0;
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++)
            if(cells[r][c].color==col && cells[r][c].type!=NONE) n++;
    return n;
}

PerftResult Board::perft(int depth) {
    if(depth == 0) return {1, 0, 0, 0, 0};

    PerftResult total;
    std::vector<Move> moves;
    legalMoves(currentPlayer(), moves);

    for(auto& m : moves) {
        Board saved = *this;
        applyMove(m);
        PerftResult res = perft(depth - 1);
        *this = saved;

        total.nodes += res.nodes;
        // In a true perft we'd track more, but nodes is most important
    }
    return total;
}
