#include "chess4.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  Time management
// ─────────────────────────────────────────────────────────────────────────────
bool Engine::timesUp() const {
    return std::chrono::steady_clock::now() >= deadline_;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Move ordering
// ─────────────────────────────────────────────────────────────────────────────
// MVV-LVA table: Most Valuable Victim - Least Valuable Aggressor
static int mvvLva(PieceType attacker, PieceType victim){
    return PieceVal::val[(int)victim] * 10 - PieceVal::val[(int)attacker];
}

void Engine::orderMoves(std::vector<Move>& moves, const Board& b, const Move& ttMove, int ply){
    // Score each move
    auto score=[&](const Move& m) -> int {
        // TT best move first
        if(m==ttMove) return 100000;

        Piece victim = b.at(m.tr,m.tc);
        Piece mover  = b.at(m.sr,m.sc);

        if(!victim.empty()){
            // Capture: MVV-LVA
            return 50000 + mvvLva(mover.type, victim.type);
        }
        if(m.promotion!=NONE){
            return 40000 + PieceVal::val[(int)m.promotion];
        }

        // Killer moves
        if(ply < 64){
            if(m == killerMoves[ply][0]) return 35000;
            if(m == killerMoves[ply][1]) return 34000;
        }

        // Quiet move: PST delta
        int pstBefore = Board::pst(mover.type, mover.color, m.sr, m.sc);
        int pstAfter  = Board::pst(mover.type, mover.color, m.tr, m.tc);
        return pstAfter - pstBefore;
    };

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b2){
        return score(a) > score(b2);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Quiescence search: only explore captures/promotions
// ─────────────────────────────────────────────────────────────────────────────
int Engine::quiesce(Board& b, int alpha, int beta){
    nodes_++;
    int stand_pat = Eval::evaluate(b, aiColor);
    if(stand_pat >= beta) return beta;
    if(stand_pat > alpha) alpha = stand_pat;

    // Only generate captures & promotions
    Color col = b.currentPlayer();
    if(col==NO_COLOR||b.ps[col].eliminated) return alpha;

    std::vector<Move> moves;
    b.genAllMoves(col, moves);

    // Filter captures only
    std::vector<Move> caps;
    caps.reserve(moves.size());
    for(auto& m:moves){
        Piece t = b.at(m.tr,m.tc);
        if(!t.empty() && t.color!=col) caps.push_back(m);
        else if(m.promotion!=NONE) caps.push_back(m);
    }
    Move dummy{}; orderMoves(caps,b,dummy, 0);

    for(auto& m:caps){
        if(timesUp()) return alpha;
        Board saved = b.clone();
        b.applyMove(m);
        int score = -quiesce(b, -beta, -alpha);
        b = saved;
        if(score>=beta) return beta;
        if(score>alpha) alpha=score;
    }
    return alpha;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paranoid alpha-beta search
//  Strategy: AI maximises its own score, ALL opponents are treated as colluding
//  against the AI (paranoid model). This is the strongest known approach for
//  FFA chess since it's the most conservative against worst-case opponents.
// ─────────────────────────────────────────────────────────────────────────────
int Engine::paranoidSearch(Board& b, int depth, int alpha, int beta, Color /*perspective*/, int ply){
    nodes_++;
    if(timesUp()) return Eval::evaluate(b, aiColor);

    // Terminal conditions
    Color current = b.currentPlayer();
    if(current==NO_COLOR) return Eval::evaluate(b,aiColor);

    // Skip eliminated players
    if(b.ps[current].eliminated){
        // Advance to next
        Board tmp=b.clone();
        int nextIdx=(tmp.turnIdx+1)%4;
        while(tmp.ps[tmp.turnOrder[nextIdx]].eliminated && nextIdx!=tmp.turnIdx)
            nextIdx=(nextIdx+1)%4;
        tmp.turnIdx=nextIdx;
        return paranoidSearch(tmp, depth, alpha, beta, (Color)tmp.turnOrder[nextIdx], ply);
    }

    // Leaf node
    if(depth<=0){
        return quiesce(b, alpha, beta);
    }

    // TT lookup
    uint64_t h = b.hash;
    size_t idx = h & (TT_SIZE-1);
    TTEntry& tte = g_tt[idx];
    Move ttMove{};
    if(tte.hash==h && tte.depth>=depth){
        if(tte.flag==TT_EXACT) return tte.score;
        if(tte.flag==TT_LOWER && tte.score>alpha) alpha=tte.score;
        if(tte.flag==TT_UPPER && tte.score<beta)  beta=tte.score;
        if(alpha>=beta) return tte.score;
        ttMove=tte.best;
    } else if(tte.hash==h) {
        ttMove=tte.best;
    }

    // Generate legal moves
    std::vector<Move> moves;
    moves.reserve(80);
    b.legalMoves(current, moves);

    if(moves.empty()){
        // Stalemate or checkmate
        if(b.isInCheck(current)){
            // Checkmated — very bad if it's AI, great if it's opponent
            return current==aiColor ? -INF/2 + (10-depth) : INF/2 - (10-depth);
        }
        return Eval::evaluate(b,aiColor); // stalemate
    }

    orderMoves(moves, b, ttMove, ply);

    bool maximizing = (current == aiColor);
    int bestScore = maximizing ? (-INF) : (INF);
    Move bestMove = moves[0];

    for(auto& m : moves){
        if(timesUp()) break;
        Board saved = b.clone();
        b.applyMove(m);

        // Check for checkmate bonus in evaluation
        for(int oc=1;oc<=4;oc++){
            Color opp=(Color)oc;
            if(opp==current || b.ps[opp].eliminated) continue;
            if(b.isCheckmated(opp)){
                b.ps[opp].eliminated=true;
                b.ps[current].score+=20;
            }
        }

        int score = paranoidSearch(b, depth-1, alpha, beta,
                                   (Color)b.turnOrder[b.turnIdx], ply + 1);
        b = saved;

        if(maximizing){
            if(score>bestScore){
                bestScore=score;
                bestMove=m;
            }
            if(score>alpha) alpha=score;
        } else {
            // All opponents minimise AI score
            if(score<bestScore){
                bestScore=score;
                bestMove=m;
            }
            if(score<beta) beta=score;
        }
        if(alpha>=beta){
            // Store killer move if quiet
            if(b.at(m.tr, m.tc).empty() && ply < 64){
                if(!(m == killerMoves[ply][0])){
                    killerMoves[ply][1] = killerMoves[ply][0];
                    killerMoves[ply][0] = m;
                }
            }
            break; // cutoff
        }
    }

    // TT store
    TTEntry ne;
    ne.hash=h; ne.score=bestScore; ne.depth=(int8_t)depth; ne.best=bestMove;
    if(bestScore<=alpha)      ne.flag=TT_UPPER;
    else if(bestScore>=beta)  ne.flag=TT_LOWER;
    else                      ne.flag=TT_EXACT;
    g_tt[idx]=ne;

    return bestScore;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Iterative deepening
// ─────────────────────────────────────────────────────────────────────────────
SearchResult Engine::search(Board& b){
    deadline_ = std::chrono::steady_clock::now()
              + std::chrono::milliseconds(timeLimit_ms);
    nodes_ = 0;

    // Make sure it's AI's turn (or search for AI's best move)
    SearchResult result;

    // Generate root moves
    std::vector<Move> rootMoves;
    b.legalMoves(aiColor, rootMoves);
    if(rootMoves.empty()) return result;

    // Clear killers for new search
    memset(killerMoves, 0, sizeof(killerMoves));

    // Pre-order root moves
    Move dummy{};
    orderMoves(rootMoves, b, dummy, 0);
    result.bestMove = rootMoves[0];

    for(int depth=1; depth<=maxDepth; depth++){
        if(timesUp()) break;

        int bestScore = -INF;
        Move bestMove = rootMoves[0];

        for(auto& m : rootMoves){
            if(timesUp()) break;
            Board saved = b.clone();
            b.applyMove(m);

            // Check checkmates from this move
            for(int oc=1;oc<=4;oc++){
                Color opp=(Color)oc;
                if(opp==aiColor || b.ps[opp].eliminated) continue;
                if(b.isCheckmated(opp)){
                    b.ps[opp].eliminated=true;
                    b.ps[aiColor].score+=20;
                }
            }

            int score = paranoidSearch(b, depth-1, -INF, INF,
                                       (Color)b.turnOrder[b.turnIdx], 1);
            b = saved;

            if(score>bestScore){
                bestScore=score;
                bestMove=m;
            }
        }

        if(!timesUp()){
            result.bestMove = bestMove;
            result.score    = bestScore;
            result.depth    = depth;
            result.nodes    = nodes_;
            // Re-order so best move is first (for next iteration)
            auto it = std::find(rootMoves.begin(), rootMoves.end(), bestMove);
            if(it!=rootMoves.end()) std::rotate(rootMoves.begin(), it, it+1);
        }

        std::cerr << "depth=" << depth
                  << " score=" << bestScore
                  << " nodes=" << nodes_ << "\n";
    }

    return result;
}
