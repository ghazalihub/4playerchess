# Chess4 — 4-Player FFA Chess Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> **1 AI (Red) vs 3 Humans (Black, Green, Blue)**  
> **Free-for-all · Point-based scoring · 14×14 board · C++17**

Chess4 is a high-performance terminal-based **4-player Free-for-All chess engine**. Unlike traditional 2-player chess, Chess4 is designed for the chaotic dynamics of multi-player warfare, where diplomacy, targeting the leader, and multi-directional king safety are paramount.

---

## Table of Contents

1. [Overview](#overview)
2. [The Board](#the-board)
3. [Players & Piece Layout](#players--piece-layout)
4. [Rules & Scoring](#rules--scoring)
5. [Building](#building)
6. [Running the Engine](#running-the-engine)
7. [GUI (Python)](#gui-python)
8. [How to Play (Commands)](#how-to-play-commands)
8. [Move Notation](#move-notation)
9. [Engine Architecture](#engine-architecture)
10. [AI Strategy & Theory](#ai-strategy--theory)
11. [Machine Protocol](#machine-protocol)
12. [Tuning & Extending](#tuning--extending)
13. [Known Limitations & Roadmap](#known-limitations--roadmap)

---

## Overview

Chess4 is a terminal-based **4-player Free-for-All chess engine** built in C++17. The game is played on a standard 14×14 four-player chess board (with 3×3 corner cutoffs). One player — **Red** — is controlled by the engine's AI. The other three players — **Black**, **Green**, and **Blue** — are human-controlled from the same terminal, taking turns.

The goal is not to checkmate a single opponent but to **accumulate the most points** by capturing pieces and checkmating enemies. The last player standing, or the player with the highest score when all others are eliminated, wins.

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
- The **four 3×3 corner regions** are inactive.
- Active squares: **148 total**.

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
- All standard piece moves (pawn double push, castling, promotion, **en passant**).
- **Checkmate:** Eliminated from the game.
- **Stalemate:** Current player stays in game but skips turn if no legal moves exist.
- **Draw Rules:** 50-move rule and 3-fold repetition are fully implemented.

### Scoring System

| Event | Points Awarded |
|-------|---------------|
| Capture a Pawn | **+1** |
| Capture a Knight/Bishop | **+3** |
| Capture a Rook | **+5** |
| Capture a Queen | **+9** |
| Deliver Checkmate | **+20** bonus |

---

## Building

### Requirements
- C++17 compatible compiler (`g++ 7+` or `clang++ 5+`)
- `make`

### Quick Build
```bash
make
```
This produces the `chess4` binary.

---

## GUI (Python)

A Pygame-based GUI is provided for better visualization and interactive play.

### Requirements
- Python 3.x
- `pygame-ce` (or `pygame`)

### Launching the GUI
```bash
make gui
# OR
python3 gui.py
```
- **Click** to select and move pieces.
- Press **Space** to trigger an AI move (Red).

---

## Running the Engine

```bash
./chess4 --depth 5 --time 5000
```

| Flag | Default | Description |
|------|---------|-------------|
| `--depth N` | `5` | Maximum search depth for the AI |
| `--time MS` | `5000` | Time limit per AI move in milliseconds |

---

## How to Play (Commands)

| Command | Description |
|---------|-------------|
| `<move>` | Enter a move in numeric or algebraic format (see below) |
| `moves` | List all legal moves for the current player |
| `board` | Redisplay the board |
| `scores` | Show current scores and player status |
| `depth N` | Change AI search depth mid-game |
| `time N` | Change AI time limit mid-game (milliseconds) |
| `resign` | Current player resigns (eliminated) |
| `help` | Show command reference |
| `quit` | Exit the program |

---

## Move Notation

### Numeric Format
`row col row col` — e.g., `1 7 3 7` (Black pawn forward two).

### Algebraic Format
`<from_col><from_row><to_col><to_row>` — e.g., `h2h4`.
Columns are `a-n` (0-13), rows are `1-14` (0-13).

---

## Engine Architecture

### 1. Board Representation
- **Zobrist Hashing:** Incrementally updated 64-bit hashes for lightning-fast Transposition Table lookups.
- **Efficient Attack Detection:** Custom `isAttacked()` logic allows the engine to verify king safety and piece threats without expensive move generation.

### 2. Search Algorithm: Paranoid Alpha-Beta
- **Paranoid Model:** Assumes all three opponents are colluding against the AI.
- **Iterative Deepening:** Increases search depth incrementally to ensures the best move is always ready.
- **Quiescence Search:** Prevents the "horizon effect" by searching until tactical captures are resolved.
- **Transposition Table:** Stores 4 million+ searched positions.
- **Killer Heuristic:** Ply-indexed killer moves to improve pruning.

### 3. Evaluation Strategy
- **Material & Position:** Piece-Square Tables (PST) optimized for the 14x14 board.
- **FFA Dynamics:** Specifically penalizes the strongest opponent ("Target the Leader") and values accumulated game points.
- **Advanced King Safety:** Penalizes exposure to multiple opponents and rewards pawn shields.

---

## Machine Protocol

Chess4 supports a strict machine-readable protocol for GUI and tool integration, enabled via the `--proto` flag.

### Input Commands
- `proto`: Handshake command. Engine responds with `protook`.
- `ready`: Check if engine is ready. Responds with `readyok`.
- `position startpos moves <moves...>`: Setup the board.
- `go`: Trigger AI search and move for the current player.
- `board`: Request full board dump. Responds with multiple `row col color type` lines followed by `boardok`.
- `<move>`: Send a human move (e.g., `h2h4`). Engine responds with `moveok` or `illegal`.

### Output Tokens
- `protook`: Initial handshake success.
- `readyok`: Engine is ready for commands.
- `moveok`: Move accepted and applied.
- `illegal`: Move rejected.
- `bestmove <move>`: AI's chosen move in algebraic format.
- `turn <color>`: Explicit notification of the current player's turn.
- `gameover <winner>`: Game has ended with the specified winner.
- `draw`: Game ended in a draw.

---

## AI Strategy & Theory

In FFA games, the **Paranoid Model** reduces the N-player game to a 2-player minimax tree. While conservative, it guarantees the AI plays safely against coordinated attacks, which is essential in a point-based FFA environment.

---

*Engine built in C++17. Developed with ❤️ for the ultimate terminal chess experience.*
