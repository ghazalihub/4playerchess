# Chess4 — 4-Player FFA Chess Engine

> **1 AI (Red) vs 3 Humans (Black, Green, Blue)**  
> Free-for-all · Point-based scoring · 14×14 board · Written in C++17

---

## Table of Contents

1. [Overview](#overview)
2. [The Board](#the-board)
3. [Players & Piece Layout](#players--piece-layout)
4. [Rules & Scoring](#rules--scoring)
5. [Building](#building)
6. [Running the Engine](#running-the-engine)
7. [How to Play (Commands)](#how-to-play-commands)
8. [Move Notation](#move-notation)
9. [Engine Architecture](#engine-architecture)
   - [File Structure](#file-structure)
   - [Board Representation](#board-representation)
   - [Move Generation](#move-generation)
   - [Zobrist Hashing](#zobrist-hashing)
   - [Transposition Table](#transposition-table)
   - [Evaluation Function](#evaluation-function)
   - [Search Algorithm](#search-algorithm)
   - [Move Ordering](#move-ordering)
   - [Iterative Deepening](#iterative-deepening)
   - [Time Management](#time-management)
10. [AI Strategy & Theory](#ai-strategy--theory)
11. [Tuning & Extending](#tuning--extending)
12. [Known Limitations & Roadmap](#known-limitations--roadmap)

---

## Overview

Chess4 is a terminal-based **4-player Free-for-All chess engine** built in C++17. The game is played on a standard 14×14 four-player chess board (with 3×3 corner cutoffs). One player — **Red** — is controlled by the engine's AI. The other three players — **Black**, **Green**, and **Blue** — are human-controlled from the same terminal, taking turns.

The goal is not to checkmate a single opponent but to **accumulate the most points** by capturing pieces and checkmating enemies. The last player standing, or the player with the highest score when all others are eliminated, wins.

This is a fundamentally different challenge from classical two-player chess engines. With four players, the game tree branches in all directions, king safety becomes multi-directional, and the optimal strategy shifts dynamically based on who the current leader is.

---

## The Board

```
     0  1  2  3  4  5  6  7  8  9 10 11 12 13
   ──────────────────────────────────────────
0 |  ░  ░  ░  R  N  B  Q  K  B  N  R  ░  ░  ░  |  ← BLACK (moves ↓)
1 |  ░  ░  ░  P  P  P  P  P  P  P  P  ░  ░  ░  |
2 |  ░  ░  ░  .  .  .  .  .  .  .  .  ░  ░  ░  |
3 |  R  P  .  .  .  .  .  .  .  .  .  .  P  R  |
4 |  N  P  .  .  .  .  .  .  .  .  .  .  P  N  |
5 |  B  P  .  .  .  .  .  .  .  .  .  .  P  B  |
6 |  Q  P  .  .  .  .  .  .  .  .  .  .  P  Q  |
7 |  K  P  .  .  .  .  .  .  .  .  .  .  P  K  |
8 |  B  P  .  .  .  .  .  .  .  .  .  .  P  B  |
9 |  N  P  .  .  .  .  .  .  .  .  .  .  P  N  |
10|  R  P  .  .  .  .  .  .  .  .  .  .  P  R  |
11|  ░  ░  ░  .  .  .  .  .  .  .  .  ░  ░  ░  |
12|  ░  ░  ░  P  P  P  P  P  P  P  P  ░  ░  ░  |
13|  ░  ░  ░  R  N  B  Q  K  B  N  R  ░  ░  ░  |  ← BLUE (moves ↑)
     ↑                                   ↑
   GREEN                                RED
  (moves →)                          (moves ←)
```

- The board is **14 columns × 14 rows**, indexed `(row, col)` from `(0,0)` at the top-left.
- The **four 3×3 corner regions** are inactive — no pieces can occupy them.
- Active squares: **148 total** (196 minus 4 × 12 corner squares).

---

## Players & Piece Layout

| Player  | Color    | Starting Side | Pawn Direction | Promotion Row/Col |
|---------|----------|---------------|----------------|-------------------|
| **Red** | 🔴 AI    | Right (col 13) | Moves LEFT (←) | Reaches col 6     |
| **Black** | ⚫ Human | Top (row 0)   | Moves DOWN (↓) | Reaches row 7     |
| **Green** | 🟢 Human | Left (col 0)  | Moves RIGHT (→)| Reaches col 7     |
| **Blue** | 🔵 Human | Bottom (row 13)| Moves UP (↑)  | Reaches row 6     |

Each player starts with the standard 8-piece back row (`R N B Q K B N R`) and 8 pawns in front of it. Turn order is: **Red → Black → Green → Blue → Red → ...**

---

## Rules & Scoring

### Standard Chess Rules Apply
- All standard piece moves (pawn double push from starting rank, castling, promotion to Queen on reaching the 8th rank equivalent).
- A player is **in check** if their king is attacked by any active opponent.
- A player is **checkmated** if they are in check and have no legal moves — they are eliminated from the game.
- **Stalemate** (not in check but no legal moves) counts as a draw for that player — they stay in the game with current score (no elimination).

### Scoring System

| Event | Points Awarded |
|-------|---------------|
| Capture a Pawn | **+1** |
| Capture a Knight | **+3** |
| Capture a Bishop | **+3** |
| Capture a Rook | **+5** |
| Capture a Queen | **+9** |
| Deliver Checkmate | **+20** bonus to the checkmating player |

Points accumulate throughout the game. **The player with the highest score when all opponents are eliminated wins.**

### Winning
- If only one player remains, they win regardless of score.
- If multiple players are simultaneously eliminated (which cannot happen mechanically, but in edge cases), the player with the highest score wins.

---

## Building

### Requirements
- C++17 compatible compiler (`g++ 7+` or `clang++ 5+`)
- `make`
- Linux or macOS (Windows via WSL)

### Quick Build
```bash
cd chess4
make
```
This compiles with full optimisations (`-O3 -march=native`) and produces the `chess4` binary.

### Debug Build (with AddressSanitizer + UBSan)
```bash
make debug
```

### Profile Build (for gprof)
```bash
make profile
./chess4
gprof chess4 gmon.out > analysis.txt
```

### Clean
```bash
make clean
```

---

## Running the Engine

```bash
./chess4                        # Default: depth 5, 5 second time limit
./chess4 --depth 7              # Stronger AI (deeper search)
./chess4 --time 10000           # 10 seconds per AI move
./chess4 --depth 3 --time 1000  # Faster/weaker for testing
./chess4 --help                 # Show CLI options
```

**CLI Options:**

| Flag | Default | Description |
|------|---------|-------------|
| `--depth N` | `5` | Maximum search depth for the AI |
| `--time MS` | `5000` | Time limit per AI move in milliseconds |

---

## How to Play (Commands)

Once the engine starts, **Red moves first automatically** (AI). Then each human player is prompted in turn.

| Command | Description |
|---------|-------------|
| `<move>` | Enter a move in numeric or algebraic format (see below) |
| `moves` | List all legal moves for the current player |
| `board` | Redisplay the board |
| `scores` | Show current scores and player status |
| `depth N` | Change AI search depth mid-game |
| `time N` | Change AI time limit mid-game (milliseconds) |
| `resign` | Current player resigns (eliminated, turn skipped forever) |
| `help` | Show command reference |
| `quit` / `exit` | Exit the program |

The AI prints search info to stderr: `depth=N score=S nodes=N` — this tells you how deep it searched and what score it evaluated.

---

## Move Notation

Two formats are accepted:

### Numeric Format (easiest)
```
row col row col
```
**Example:** `1 7 3 7` — move piece at row 1, col 7 to row 3, col 7 (Black's center pawn forward two squares).

### Algebraic Format
```
<from_col_letter><from_row_number><to_col_letter><to_row_number>
```
Columns are lettered `a` through `n` (a=0, b=1, ... n=13). Rows are numbered 1 through 14 (1=row 0, 14=row 13).

**Example:** `h2h4` — col h (=7), row 2 (=row 1) to col h (=7), row 4 (=row 3). Same move as above.

**Promotion:** append the piece letter: `h7h8q` (promotes to queen).

### Column Letter Reference
```
Col:  0  1  2  3  4  5  6  7  8  9  10  11  12  13
      a  b  c  d  e  f  g  h  i  j   k   l   m   n
```

### Example Opening Moves

**Black** (top, moves down):
- `1 7 3 7` or `h2h4` — center pawn, double push
- `0 4 2 3` or `e1d3` — knight development

**Green** (left, moves right):
- `7 0 7 2` or `a8c8` — king pawn push (Green's king is at row 7)
- `3 0 5 1` or `a4b6` — ... (check `moves` for the actual list)

**Blue** (bottom, moves up):
- `12 7 10 7` or `h13h11` — center pawn, double push
- `13 4 11 5` or `e14f12` — knight development

---

## Engine Architecture

### File Structure

```
chess4/
├── chess4.hpp    — All types, structs, class declarations, constants
├── board.cpp     — Board representation, Zobrist hashing, move generation,
│                   legal move filtering, apply/undo, check/checkmate detection
├── eval.cpp      — Static evaluation: material, PST, mobility, king safety,
│                   pawn structure, FFA-specific multi-player scoring
├── search.cpp    — Paranoid alpha-beta, quiescence search, iterative deepening,
│                   move ordering (MVV-LVA + TT + PST delta), time management
├── game.cpp      — Interactive terminal UI, board display (ANSI colour),
│                   move parsing (numeric + algebraic), game loop
├── main.cpp      — Entry point, CLI argument parsing
└── Makefile      — Build targets: all, debug, profile, clean
```

---

### Board Representation

**File:** `chess4.hpp`, `board.cpp`

The board is a flat 2D array of `Piece` structs:

```cpp
struct Piece {
    PieceType type;   // NONE, P, N, B, R, Q, K
    Color     color;  // NO_COLOR, RED(1), BLACK(2), GREEN(3), BLUE(4)
};

Piece cells[14][14];
```

Each cell is 2 bytes. The full board state is 196 bytes — very cache-friendly.

**Corner cutoff** is handled by `inBounds(r, c)`, which returns `false` for the four 3×3 corner regions. Every function that accesses the board goes through `inBounds` so invalid squares are silently ignored.

**Player state** is tracked per-color in `PlayerState ps[5]` (indexed 1–4):

```cpp
struct PlayerState {
    int  score;         // accumulated game points
    bool eliminated;    // true after checkmate
    bool kingMoved;     // castling rights tracker
    bool rookKMoved;    // kingside rook moved
    bool rookQMoved;    // queenside rook moved
};
```

**Turn management:** `turnOrder[4]` holds the fixed rotation `{RED, BLACK, GREEN, BLUE}`. `turnIdx` advances each move, skipping eliminated players automatically.

---

### Move Generation

**File:** `board.cpp` — `genMovesFor()`, `genAllMoves()`, `legalMoves()`

Generation is split into two stages:

**Stage 1 — Pseudo-legal moves** (`genMovesFor` / `genAllMoves`):  
Generates all geometrically valid moves without checking whether they leave the king in check. This is fast and handles:
- **Pawns:** directional per player (each of the four colors has a unique forward direction). Double push from starting rank. Diagonal captures. Auto-promotes to Queen on reaching the opponent's back-rank threshold.
- **Knights:** standard L-shapes, bounds-checked.
- **Bishops:** diagonal ray sliding, blocked by first piece encountered.
- **Rooks:** orthogonal ray sliding.
- **Queens:** combined rook + bishop rays.
- **Kings:** single-step all directions. Castling: checks that the king and relevant rook haven't moved, and that the squares between them are empty.

**Stage 2 — Legal moves** (`legalMoves`):  
For each pseudo-legal move, clones the board, applies the move, then calls `isInCheck(col)` on the moving player. Moves that leave the own king in check are discarded.

**Check detection** (`isInCheck`): finds the king's square, then generates all pseudo-legal moves for every active opponent and checks if any of them land on the king.

---

### Zobrist Hashing

**File:** `board.cpp`, `chess4.hpp`

Incrementally maintained 64-bit hash of the board state, used as the key for the transposition table.

```cpp
// Table: [color][piece_type][row][col] → random uint64
uint64_t table[5][7][14][14];
```

Initialized once with `Zobrist::init()` using a fixed-seed Mersenne Twister (`0xDEADBEEFCAFEBABE`) for full reproducibility. Every call to `Board::set(r, c, piece)` XORs the hash with the old piece's entry (removing it) and the new piece's entry (adding it), keeping the hash perfectly in sync with no full recomputation.

---

### Transposition Table

**File:** `chess4.hpp`, `search.cpp`

A direct-mapped hash table with **4,194,304 entries** (2²² = 4M), consuming ~80 MB of memory.

```cpp
struct TTEntry {
    uint64_t hash;    // full hash for collision detection
    int      score;   // stored score
    int8_t   depth;   // depth at which this was searched
    TTFlag   flag;    // TT_EXACT, TT_LOWER (alpha), TT_UPPER (beta)
    Move     best;    // best move found at this node
};
```

**Lookup:** `index = hash & (TT_SIZE - 1)`. If `entry.hash == board.hash && entry.depth >= current_depth`, the stored score is used (exact) or used to tighten alpha/beta bounds (lower/upper).

**The TT best move** is always extracted even on shallow hits and used as the first move tried in move ordering — this is one of the strongest pruning techniques.

---

### Evaluation Function

**File:** `eval.cpp` — `Eval::evaluate()`

The evaluation is entirely from the **AI's perspective** (Red). A higher score means a better position for Red. It is composed of six weighted terms:

#### 1. Own Material
```
sum of PieceVal::val[type] for all Red pieces
```
Piece values (centipawns): P=100, N=320, B=330, R=500, Q=900, K=20000

#### 2. Piece-Square Tables (PST)
Each piece type has a positional bonus based on where it sits on the board. All tables are orientation-aware — the same function gives the correct bonus for each player's direction of travel.

| Piece | Positional Bonus |
|-------|-----------------|
| **Pawn** | Graduated advancement bonus (-10 near start → +40 near promotion) |
| **Knight** | Center proximity + outpost bonus for being deep in enemy territory |
| **Bishop** | Center proximity + bonus for being past halfway |
| **Rook** | Mild center bonus + large bonus for advanced rank |
| **Queen** | Center proximity (wants to be central but not too early) |
| **King** | Penalty proportional to how far it has advanced (stay safe!) |

The "center" is computed as Euclidean distance from `(6.5, 6.5)`. The maximum distance on the board is ~9.2; the bonus formula is `scale × (9.0 - distance)`.

#### 3. Game Score Bonus
```
aiScore += board.ps[RED].score × 15
```
Rewards accumulated game points (captures + checkmate bonuses already earned) so the AI values the scoring system, not just material on the board.

#### 4. King Safety
For Red's king, scans all opponent pseudo-legal moves:
- **+50 penalty** per move that directly attacks the king square
- **+15 penalty** per move that lands adjacent to the king (ring of 8 squares)
- **+10 bonus** per own pawn forming a shield behind the king

#### 5. Pawn Structure
- **−20 per doubled pawn** (two pawns on the same file/rank for that color)
- **+30 + (advancement × 5) per passed pawn** (no enemy pawn blocking on adjacent lanes ahead)

#### 6. Opponent Suppression
For each active opponent:
```
aiScore -= (oppMaterial + oppPST + oppKingSafety + oppPawnStructure) / 3
```
This means Red benefits from opponents being in weak positions. The **strongest opponent** (by material) gets an extra penalty — this implements the classic FFA strategy of "attack the leader."

**Endgame mode** (only one opponent left): the evaluation shifts to purely maximising Red's material advantage over the remaining opponent.

---

### Search Algorithm

**File:** `search.cpp`

#### Paranoid Alpha-Beta

The core search uses the **Paranoid model** — the standard and theoretically strongest algorithm for FFA multi-player games.

The paranoid assumption is: **all opponents are assumed to be colluding against the AI**. This means:

- When it's Red's turn → **maximise** Red's score (standard max node)
- When it's any other player's turn → **minimise** Red's score (all opponents are treated as a single MIN player)

This reduces the multi-player problem to a standard two-player minimax tree, allowing full alpha-beta pruning. While opponents in a real game would sometimes make moves that hurt each other rather than hurting Red, the paranoid model guarantees Red always plays safely against the **worst case**.

```
paranoidSearch(board, depth, alpha, beta, perspective):
    if depth == 0 → quiesce(board, alpha, beta)
    if current_player == RED → maximising node
    else → minimising node (treat all opponents as one enemy)
    for each legal move:
        apply move
        score = paranoidSearch(board, depth-1, alpha, beta, next_player)
        undo move
        update alpha/beta, prune if alpha >= beta
```

#### Quiescence Search

At leaf nodes (depth=0), instead of returning the static evaluation immediately, the engine continues searching **captures and promotions only** until the position is "quiet" (no more captures available). This eliminates the **horizon effect** — the engine won't evaluate a position as good right before an obvious piece loss just because the capture is one ply beyond the depth limit.

```
quiesce(board, alpha, beta):
    stand_pat = evaluate(board)
    if stand_pat >= beta → return beta (fail-high)
    alpha = max(alpha, stand_pat)
    for each capture/promotion:
        apply move
        score = -quiesce(board, -beta, -alpha)
        undo move
        update alpha, prune if alpha >= beta
```

---

### Move Ordering

**File:** `search.cpp` — `Engine::orderMoves()`

Good move ordering is critical for alpha-beta efficiency. Moves are scored and sorted descending before searching:

| Priority | Move Type | Score |
|----------|-----------|-------|
| 1st | TT best move (from transposition table) | 100,000 |
| 2nd | Captures (MVV-LVA ordered) | 50,000 + MVV-LVA |
| 3rd | Promotions | 40,000 + piece value |
| 4th | Quiet moves | PST(after) − PST(before) |

**MVV-LVA (Most Valuable Victim – Least Valuable Aggressor):**  
Captures are sorted by `victim_value × 10 − attacker_value`. This means a pawn capturing a queen is tried before a queen capturing a pawn — capturing expensive pieces with cheap pieces first is typically best.

---

### Iterative Deepening

**File:** `search.cpp` — `Engine::search()`

The engine uses **iterative deepening**: it runs a complete paranoid alpha-beta search at depth 1, then depth 2, then 3, up to `maxDepth`, stopping early if time runs out.

Benefits:
- The best move from the previous depth iteration is moved to the front of the root move list for the next iteration, dramatically improving alpha-beta pruning efficiency.
- If time runs out mid-search, the last *fully completed* depth's best move is returned — always a valid result.
- Deeper searches re-use the transposition table populated by shallower searches.

```
for depth in 1..maxDepth:
    if time_up → break
    for each root move:
        apply, search depth-1, undo
    update best_move from this depth
    rotate best_move to front of root list
return last fully completed best_move
```

---

### Time Management

**File:** `search.cpp` — `Engine::timesUp()`

A `std::chrono::steady_clock` deadline is set at the start of `search()`:
```cpp
deadline_ = steady_clock::now() + milliseconds(timeLimit_ms);
```

`timesUp()` is checked at the top of every `paranoidSearch` call and inside the quiescence loop. When time expires, the search unwinds and returns the evaluation at the current node — the root always has at least depth-1's best move to fall back on.

---

## AI Strategy & Theory

### Why Paranoid?

In FFA multi-player games, several search models exist:

| Model | Description | Problem |
|-------|-------------|---------|
| **Max^n** | Each player maximises their own score | Requires N-dimensional alpha-beta bounds; hard to prune effectively |
| **Paranoid** | AI maximises; all others minimise AI | Reduces to 2-player minimax; full alpha-beta works; conservative but strong |
| **Soft Paranoid** | Weighted blend of Max^n and Paranoid | Theoretically better but harder to implement well |

Chess4 uses **Paranoid** because it produces the best practical results at playable depths. The engine will assume the worst from all three opponents, which means it never walks into a trap even if two opponents are actually fighting each other.

### FFA-Specific Strategies the Eval Encodes

**Target the leader:** The evaluation penalises the strongest opponent twice — once through their raw strength (opponent suppression) and again through the "strongest opponent" bonus. This teaches the AI to gang up on whoever is winning, exactly as strong human FFA players do.

**Score-awareness:** Points already earned are valued at `score × 15`. The AI therefore prioritises completing captures over positional maneuvering when it has a chance to score.

**King safety in all directions:** Unlike 2-player chess where you only worry about one opponent's attacks, king safety here aggregates threats from all three opponents simultaneously.

**Passed pawns matter more:** A passed pawn in 4-player chess is extremely dangerous because it approaches promotion without necessarily having any opposing pawns to stop it (other players' pawns move in different directions). The passed pawn bonus grows with advancement to reflect this.

---

## Tuning & Extending

### Changing Piece Values
Edit `PieceVal::val[]` in `chess4.hpp`:
```cpp
static constexpr int val[7] = { 0, 100, 320, 330, 500, 900, 20000 };
//                                 P    N    B    R    Q    K
```

### Changing Evaluation Weights
All eval weights are in `eval.cpp`. Key constants:
- `centerBonus(r, c, scale)` — the `scale` parameter controls how strongly pieces prefer the center
- `pawnAdvBonus` thresholds — controls how aggressively pawns push
- `kingSafety` penalty values: `15` (adjacent attack), `50` (direct attack), `10` (shield bonus)
- `pawnStructure` values: `-20` (doubled), `+30 + adv×5` (passed)
- `aiScore -= oppTotal / 3` — the `3` controls how much the AI suppresses opponents
- `b.ps[aiColor].score * 15` — the `15` controls how much game-score is valued vs board material

### Adding En Passant
`board.cpp` — `genMovesFor()` handles pawns. Add an `enPassantSq` field to `Board` and check for it in the pawn section.

### Adding Better King Safety
Replace the current proximity-based king safety with a proper **pawn storm / open file** detector for the square in front of the king.

### UCI-like Protocol
`game.cpp` currently uses a human-readable protocol. To make Chess4 interoperable with other tools, implement a simple text protocol that reads `position` and `go depth N` commands from stdin.

---

## Known Limitations & Roadmap

| Limitation | Notes |
|------------|-------|
| No en passant | Not implemented — rarely matters at this level |
| Simplified castling | Castling rights are tracked but the path-not-attacked check is simplified |
| No 50-move rule | Games cannot draw by repetition or 50 moves |
| Board copy for undo | Uses full board clone instead of true incremental undo (slightly slower) |
| Quiescence uses negamax sign flip | Works correctly for AI perspective but uses a sign flip that assumes 2-player semantics — fine for paranoid model |
| No opening book | Starts searching from move 1; could add a small 4-player opening database |
| Terminal only | No GUI — board is rendered in ANSI colour in the terminal |

### Potential Future Improvements
- **Late Move Reductions (LMR):** Reduce depth on quiet moves late in the move list
- **Null move pruning:** Skip a move to detect zugzwang-free positions
- **Killer heuristic:** Cache moves that caused beta cutoffs at a given depth
- **History heuristic:** Score quiet moves by how often they caused cutoffs historically
- **True incremental undo:** Store only the delta (piece moved, piece captured) instead of cloning 196 bytes
- **Aspiration windows:** Search with a narrow alpha-beta window around the previous depth's score
- **NNUE evaluation:** Train a neural network evaluation function on self-play games

---

## Quick Reference Card

```
Board layout (row, col):
  Black:  row 0-1,  cols 3-10  — moves DOWN  ↓
  Blue:   row 12-13, cols 3-10 — moves UP    ↑
  Green:  rows 3-10, col 0-1   — moves RIGHT →
  Red:    rows 3-10, col 12-13 — moves LEFT  ←

Points: P=1  N=3  B=3  R=5  Q=9  Checkmate=+20

Commands: moves | board | scores | resign | quit
          depth N | time N | help

Move format:
  Numeric:    "row col row col"   →  3 12 3 11
  Algebraic:  "colrow colrow"     →  m4l4
  Columns:    a=0 b=1 c=2 d=3 e=4 f=5 g=6 h=7
              i=8 j=9 k=10 l=11 m=12 n=13
  Rows:       1=row0 ... 14=row13

Build:   make
Run:     ./chess4 --depth 5 --time 5000
```

---

*Engine built in C++17. AI uses Paranoid Alpha-Beta with Iterative Deepening, Quiescence Search, Transposition Table, MVV-LVA Move Ordering, and a multi-player FFA evaluation function.*
