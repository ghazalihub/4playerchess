#include "chess4.hpp"
#include <cmath>
#include <algorithm>

// Forward declare board.cpp helpers used here
static void pawnDelta(Color col, int& dr, int& dc){
    switch(col){
        case BLACK: dr=1;  dc=0;  break;
        case BLUE:  dr=-1; dc=0;  break;
        case GREEN: dr=0;  dc=1;  break;
        case RED:   dr=0;  dc=-1; break;
        default:    dr=0;  dc=0;  break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Piece-Square Tables (oriented toward center for most pieces)
//  Board is 14×14 with corner cutoffs. Center ~ (6.5,6.5)
//  Tables are symmetric and mapped per player orientation
// ─────────────────────────────────────────────────────────────────────────────

// Distance from center (6.5,6.5), higher = worse for most pieces
static float centerDist(int r, int c){
    float dr = r - 6.5f, dc = c - 6.5f;
    return std::sqrt(dr*dr + dc*dc);
}

// Generic bonus: pieces want to be near center
static int centerBonus(int r, int c, int scale=5){
    float d = centerDist(r,c);
    // Center of 14x14 is around (6.5, 6.5)
    // Max distance is sqrt(6.5^2 + 6.5^2) approx 9.19
    return (int)(scale * (9.2f - d));
}

// For a player, "advancement" = how far their pieces have moved into enemy territory
// Returns 0-13 value (higher = more advanced)
static int advancement(int r, int c, Color col){
    switch(col){
        case BLACK: return r;         // black moves down, higher row = more advanced
        case BLUE:  return 13-r;      // blue moves up
        case GREEN: return c;         // green moves right
        case RED:   return 13-c;      // red moves left
        default:    return 0;
    }
}

// Pawn advancement bonus
static int pawnAdvBonus(int r, int c, Color col){
    int adv = advancement(r,c,col);
    if(adv<=1) return -10; // start rank
    if(adv<=3) return 0;
    if(adv<=6) return 10;
    if(adv<=9) return 30;
    if(adv<=11) return 60;
    return 150; // Very close to promotion rank (13)
}

// Knight outpost: bonus for being in enemy half
static int knightBonus(int r, int c, Color col){
    int adv = advancement(r,c,col);
    int cb  = centerBonus(r,c,3);
    return cb + (adv>6 ? 15 : 0);
}

// Bishop: prefers open diagonals — proxy with center
static int bishopBonus(int r, int c, Color col){
    return centerBonus(r,c,4) + (advancement(r,c,col)>5 ? 10 : 0);
}

// Rook: wants open files/rows (center proximity + rank)
static int rookBonus(int r, int c, Color col){
    return centerBonus(r,c,2) + (advancement(r,c,col)>7 ? 20 : 0);
}

// Queen: highly valuable in center
static int queenBonus(int r, int c, Color /*col*/){
    return centerBonus(r,c,3);
}

// King: safety — stay near back rank in midgame
static int kingMidgameBonus(int r, int c, Color col){
    int adv = advancement(r,c,col);
    // Penalize for being far from back rank, especially if very far
    int penalty = adv * 10 + (adv > 2 ? (adv-2)*(adv-2)*5 : 0);
    return -penalty;
}

int Board::pst(PieceType t, Color col, int r, int c){
    switch(t){
        case P: return pawnAdvBonus(r,c,col);
        case N: return knightBonus(r,c,col);
        case B: return bishopBonus(r,c,col);
        case R: return rookBonus(r,c,col);
        case Q: return queenBonus(r,c,col);
        case K: return kingMidgameBonus(r,c,col);
        default: return 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Namespace containing all static evaluation logic.
 */
namespace Eval {

/**
 * @brief Calculates a mobility score based on the number of legal moves.
 */
int mobilityScore(Board& b, Color col){
    if(b.ps[col].eliminated) return 0;
    std::vector<Move> moves;
    b.genAllMoves(col, moves);
    return (int)moves.size() * 2;
}

/**
 * @brief Evaluates the safety of the king for a given player.
 * Considers direct attacks, proximity of enemy pieces, and pawn shields.
 */
int kingSafety(const Board& b, Color col){
    if(b.ps[col].eliminated) return 0;
    Sq king = b.findKing(col);
    if(king.r<0) return -500;

    int penalty = 0;
    // Count attackers around king
    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==col || b.ps[opp].eliminated) continue;

        // Use efficient isAttacked for direct check
        if(b.isAttacked(king.r, king.c, opp)) penalty += 100;

        // Check surrounding 8 squares
        for(int dr=-1;dr<=1;dr++){
            for(int dc=-1;dc<=1;dc++){
                if(dr==0 && dc==0) continue;
                int nr=king.r+dr, nc=king.c+dc;
                if(inBounds(nr,nc) && b.isAttacked(nr, nc, opp)){
                    penalty += 25;
                }
            }
        }
    }
    // Bonus for having pawns near king
    int shield=0;
    int dr=0,dc=0; pawnDelta(col,dr,dc);
    for(int i=-1;i<=1;i++){
        int pr = king.r - dr + (col==RED||col==GREEN?i:0);
        int pc = king.c - dc + (col==BLACK||col==BLUE?i:0);
        if(inBounds(pr,pc)){
            auto& p=b.cells[pr][pc];
            if(p.type==P && p.color==col) shield+=10;
        }
    }
    return shield - penalty;
}

int pawnStructure(const Board& b, Color col){
    if(b.ps[col].eliminated) return 0;
    int score=0;
    // For each pawn, check if it's doubled, isolated, or passed
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++){
            auto& p=b.cells[r][c];
            if(p.type!=P || p.color!=col) continue;

            // Doubled pawn penalty: another friendly pawn on same file/rank
            int doubled=0;
            if(col==BLACK||col==BLUE){
                for(int r2=0;r2<ROWS;r2++)
                    if(r2!=r && b.cells[r2][c].type==P && b.cells[r2][c].color==col) doubled++;
            } else {
                for(int c2=0;c2<COLS;c2++)
                    if(c2!=c && b.cells[r][c2].type==P && b.cells[r][c2].color==col) doubled++;
            }
            if(doubled>0) score-=20;

            // Passed pawn bonus: no enemy pawns blocking
            bool passed=true;
            int advDir=advancement(r,c,col);
            for(int oc=1;oc<=4;oc++){
                Color opp=(Color)oc;
                if(opp==col||b.ps[opp].eliminated) continue;
                for(int r2=0;r2<ROWS;r2++)
                    for(int c2=0;c2<COLS;c2++){
                        auto& op=b.cells[r2][c2];
                        if(op.type==P && op.color==opp){
                            // Rough check: enemy pawn ahead on same lane
                            if(col==BLACK||col==BLUE){
                                if(std::abs(c2-c)<=1 && advancement(r2,c2,col)>advDir) passed=false;
                            } else {
                                if(std::abs(r2-r)<=1 && advancement(r2,c2,col)>advDir) passed=false;
                            }
                        }
                    }
            }
            if(passed) score+=30+advDir*5;
        }
    return score;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main evaluation from AI's perspective (higher = better for AI)
//  In 4-player FFA, strategy is:
//   1) Maximise own material & position
//   2) Minimise the strongest opponent (threat management)
//   3) Score captures optimally
// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief The main static evaluation function.
 * @param b The board state to evaluate.
 * @param aiColor The perspective from which to evaluate (usually RED).
 * @return Score in centipawns. Positive favors aiColor.
 */
int evaluate(const Board& b, Color aiColor){
    if(b.ps[aiColor].eliminated) return -INF/2;

    // Count active opponents
    int activeOpps=0;
    for(int oc=1;oc<=4;oc++)
        if((Color)oc!=aiColor && !b.ps[(Color)oc].eliminated) activeOpps++;

    if(activeOpps==0) return INF/2; // AI wins

    int aiScore=0;
    Board& bm = const_cast<Board&>(b);

    // ── 1) Own material + PST + Tactical Hanging Check
    // We value our own pieces and their positioning (PST).
    // We also apply an immediate penalty for pieces that are under attack.
    int ownMat=0, ownPst=0;
    for(int r=0;r<ROWS;r++)
        for(int c=0;c<COLS;c++){
            if(!::inBounds(r,c)) continue;
            auto& p=b.cells[r][c];
            if(p.color==aiColor){
                int val = PieceVal::get(p.type);
                ownMat += val;
                ownPst += Board::pst(p.type, aiColor, r, c);

                // Quick tactically hanging check
                bool attacked = false;
                for(int oc=1; oc<=4; oc++){
                    if((Color)oc != aiColor && !b.ps[oc].eliminated){
                        if(bm.isAttacked(r, c, (Color)oc)){ attacked = true; break; }
                    }
                }
                if(attacked){
                    if(!bm.isAttacked(r, c, aiColor)) aiScore -= val / 2; // Hanging!
                    else aiScore -= val / 10; // Under pressure but defended
                }
            }
        }
    aiScore += (ownMat + ownPst);

    // ── 2) Game-score bonus (permanent points from past captures)
    aiScore += b.ps[aiColor].score * 40;

    // ── 3) King safety
    aiScore += kingSafety(b, aiColor);

    // ── 4) Pawn structure
    aiScore += pawnStructure(b, aiColor);

    // ── 5) Mobility
    aiScore += mobilityScore(bm, aiColor);

    // ── 6) Opponent evaluation (Suppression)
    // In FFA, we want opponents to be weak. We target them proportional to their strength.
    int strongestOppTotal=0;
    for(int oc=1;oc<=4;oc++){
        Color opp=(Color)oc;
        if(opp==aiColor || b.ps[opp].eliminated) continue;

        int oppMat=b.materialOf(opp);
        int oppPst=0;
        for(int r=0;r<ROWS;r++)
            for(int c=0;c<COLS;c++){
                if(!::inBounds(r,c)) continue;
                auto& p=b.cells[r][c];
                if(p.color==opp) oppPst+=Board::pst(p.type,opp,r,c);
            }

        // Opponent total strength includes material, safety, and their own captured points
        int oppTotal = oppMat + oppPst + kingSafety(b,opp) + b.ps[opp].score * 40;

        // Subtract opponent's strength from our perspective (Suppression)
        aiScore -= oppTotal / 3;

        if(oppTotal > strongestOppTotal) strongestOppTotal = oppTotal;

        // King Attack bonus: reward threatening an enemy king
        Sq oppKing = b.findKing(opp);
        if(oppKing.r != -1 && b.isAttacked(oppKing.r, oppKing.c, aiColor)){
            aiScore += 40;
        }
    }

    // ── 7) FFA Strategy: Target the Leader
    // If one opponent is much stronger, they are the primary threat to our victory.
    aiScore -= strongestOppTotal / 4;

    // ── 8) Endgame adjustment: if only one opponent left, play for the win.
    if(activeOpps==1){
        aiScore += ownMat - strongestOppTotal/2;
    }

    return aiScore;
}

} // namespace Eval
