import pygame
import subprocess
import sys
import threading
import queue
import time

# Constants
WIDTH, HEIGHT = 800, 800
ROWS, COLS = 14, 14
SQUARE_SIZE = WIDTH // COLS

# Colors
LIGHT_BLUE = (153, 204, 255)
DARK_BLUE = (111, 168, 220)
GRAY = (50, 50, 50)
RED = (255, 0, 0)
BLACK = (0, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
WHITE = (255, 255, 255)
YELLOW = (255, 255, 0)

class ChessEngine:
    def __init__(self):
        self.process = subprocess.Popen(
            ['./chess4', '--proto'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        self.stdout_queue = queue.Queue()
        self.stderr_queue = queue.Queue()

        threading.Thread(target=self._read_stream, args=(self.process.stdout, self.stdout_queue), daemon=True).start()
        threading.Thread(target=self._read_stream, args=(self.process.stderr, self.stderr_queue), daemon=True).start()

    def _read_stream(self, stream, q):
        for line in iter(stream.readline, ''):
            q.put(line.strip())

    def send(self, cmd):
        # print(f"DEBUG SEND: {cmd}")
        self.process.stdin.write(cmd + '\n')
        self.process.stdin.flush()

    def get_output(self):
        out = []
        while not self.stdout_queue.empty():
            out.append(self.stdout_queue.get_nowait())
        return out

def in_bounds(r, c):
    if r < 0 or r >= 14 or c < 0 or c >= 14: return False
    if r < 3 and c < 3: return False
    if r < 3 and c > 10: return False
    if r > 10 and c < 3: return False
    if r > 10 and c > 10: return False
    return True

class GUI:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption("Chess4 - 4 Player FFA")
        self.engine = ChessEngine()
        self.board = [[None for _ in range(14)] for _ in range(14)]
        self.selected = None
        self.pending_move = None
        self.turn_color = "red"
        self.winner = None

        self.engine.send("proto")
        self.engine.send("ready")
        self.sync_board()

    def sync_board(self):
        self.engine.send("board")

    def draw_board(self):
        for r in range(ROWS):
            for c in range(COLS):
                color = LIGHT_BLUE if (r + c) % 2 == 0 else DARK_BLUE
                if not in_bounds(r, c):
                    color = GRAY
                pygame.draw.rect(self.screen, color, (c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE))

                piece = self.board[r][c]
                if piece:
                    p_color_name, p_type = piece
                    p_color = RED if p_color_name == 'red' else BLACK if p_color_name == 'black' else GREEN if p_color_name == 'green' else BLUE

                    center = (c * SQUARE_SIZE + SQUARE_SIZE // 2, r * SQUARE_SIZE + SQUARE_SIZE // 2)
                    pygame.draw.circle(self.screen, p_color, center, SQUARE_SIZE // 2 - 5)
                    font = pygame.font.SysFont("Arial", 24, bold=True)
                    text = font.render(p_type, True, WHITE)
                    self.screen.blit(text, text.get_rect(center=center))

        if self.selected:
            r, c = self.selected
            pygame.draw.rect(self.screen, YELLOW, (c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE), 3)

        # Draw status
        font = pygame.font.SysFont("Arial", 20, bold=True)
        status = f"Turn: {self.turn_color.capitalize()}"
        if self.winner:
            status = f"GAME OVER! Winner: {self.winner.capitalize()}"
        text = font.render(status, True, WHITE)
        self.screen.blit(text, (10, 10))

    def to_algebraic(self, r, c):
        col = chr(ord('a') + c)
        row = str(r + 1)
        return col + row

    def handle_click(self, pos):
        if self.winner: return
        c, r = pos[0] // SQUARE_SIZE, pos[1] // SQUARE_SIZE
        if not in_bounds(r, c): return

        if self.selected:
            if self.selected == (r, c):
                self.selected = None
            else:
                move_str = self.to_algebraic(*self.selected) + self.to_algebraic(r, c)
                self.pending_move = (self.selected, (r, c))
                self.engine.send(move_str)
                self.selected = None
        else:
            piece = self.board[r][c]
            if piece and piece[0] == self.turn_color and self.turn_color != "red":
                self.selected = (r, c)

    def parse_engine_output(self):
        lines = self.engine.get_output()
        for line in lines:
            # print(f"ENGINE: {line}")
            if line == "moveok":
                self.pending_move = None
                self.sync_board()
            elif line == "illegal":
                print("Illegal move reported by engine.")
                self.pending_move = None
            elif line.startswith("bestmove"):
                self.sync_board()
            elif line.startswith("turn"):
                self.turn_color = line.split()[1]
            elif line.startswith("gameover"):
                self.winner = line.split()[1]
            elif line.startswith("boardok"):
                pass
            elif line == "board":
                # Clear board for full refresh
                self.board = [[None for _ in range(14)] for _ in range(14)]
            elif len(line.split()) == 4:
                # Board data: r c color type
                parts = line.split()
                try:
                    r, c = int(parts[0]), int(parts[1])
                    color, p_type = parts[2], parts[3]
                    self.board[r][c] = (color, p_type)
                except: pass
            elif line == "protook" or line == "readyok":
                pass

    def run(self):
        clock = pygame.time.Clock()
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    self.handle_click(pygame.mouse.get_pos())

            self.parse_engine_output()

            self.screen.fill((0, 0, 0))
            self.draw_board()
            pygame.display.flip()
            clock.tick(60)

if __name__ == "__main__":
    gui = GUI()
    gui.run()
