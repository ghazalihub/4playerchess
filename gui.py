import pygame
import subprocess
import sys
import threading
import queue

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
        self.board = self.reset_board()
        self.selected = None
        self.history = []
        self.turn_color = "Red" # Starting color
        self.engine.send("proto")

    def reset_board(self):
        # Initial piece setup (Simplified for GUI visualization)
        board = [[None for _ in range(14)] for _ in range(14)]
        back_row = ['R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R']

        # Black (Top)
        for i, p in enumerate(back_row):
            board[0][3+i] = ('Black', p)
            board[1][3+i] = ('Black', 'P')
        # Blue (Bottom)
        for i, p in enumerate(back_row):
            board[13][3+i] = ('Blue', p)
            board[12][3+i] = ('Blue', 'P')
        # Green (Left)
        for i, p in enumerate(back_row):
            board[3+i][0] = ('Green', p)
            board[3+i][1] = ('Green', 'P')
        # Red (Right)
        for i, p in enumerate(back_row):
            board[3+i][13] = ('Red', p)
            board[3+i][12] = ('Red', 'P')

        return board

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
                    p_color = RED if p_color_name == 'Red' else BLACK if p_color_name == 'Black' else GREEN if p_color_name == 'Green' else BLUE

                    # Draw piece as a circle with a letter
                    center = (c * SQUARE_SIZE + SQUARE_SIZE // 2, r * SQUARE_SIZE + SQUARE_SIZE // 2)
                    pygame.draw.circle(self.screen, p_color, center, SQUARE_SIZE // 2 - 5)
                    font = pygame.font.SysFont("Arial", 24, bold=True)
                    text = font.render(p_type, True, WHITE)
                    self.screen.blit(text, text.get_rect(center=center))

        if self.selected:
            r, c = self.selected
            pygame.draw.rect(self.screen, (255, 255, 0), (c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE), 3)

    def to_algebraic(self, r, c):
        col = chr(ord('a') + c)
        row = str(r + 1)
        return col + row

    def handle_click(self, pos):
        c, r = pos[0] // SQUARE_SIZE, pos[1] // SQUARE_SIZE
        if not in_bounds(r, c): return

        if self.selected:
            if self.selected == (r, c):
                self.selected = None
            else:
                move_str = self.to_algebraic(*self.selected) + self.to_algebraic(r, c)
                self.engine.send(move_str)
                self.history.append(move_str)
                self.apply_move_to_local_board(self.selected, (r, c))
                self.selected = None
                self.update_engine_state()
        else:
            if self.board[r][c]:
                self.selected = (r, c)

    def apply_move_to_local_board(self, src, dst):
        piece = self.board[src[0]][src[1]]
        self.board[src[0]][src[1]] = None
        self.board[dst[0]][dst[1]] = piece

    def update_engine_state(self):
        cmd = "position startpos moves " + " ".join(self.history)
        self.engine.send(cmd)

    def parse_engine_output(self):
        lines = self.engine.get_output()
        for line in lines:
            if "BESTMOVE" in line:
                # Example: ... BESTMOVE m7k7 ...
                parts = line.split()
                try:
                    idx = parts.index("BESTMOVE")
                    move_str = parts[idx+1]
                    self.history.append(move_str)

                    # Parse move_str to update local board
                    # a1b2
                    sc = ord(move_str[0]) - ord('a')
                    # find where second col letter starts
                    p2=1
                    while p2<len(move_str) and move_str[p2].isdigit(): p2+=1
                    sr = int(move_str[1:p2]) - 1
                    tc = ord(move_str[p2]) - ord('a')
                    tr = int(move_str[p2+1:]) - 1

                    self.apply_move_to_local_board((sr, sc), (tr, tc))
                    print(f"AI Move: {move_str}")
                except Exception as e:
                    print(f"Error parsing AI move: {e} in line: {line}")
            elif "move ok" in line:
                pass
            elif "Illegal move" in line:
                print("Engine reported illegal move. Rolling back history.")
                if self.history: self.history.pop()
                self.board = self.reset_board()
                # Replay history
                for h in self.history:
                    # Very simple replay
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
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_SPACE:
                        self.engine.send("go")

            self.parse_engine_output()
            # In a real GUI, we'd need to re-parse the entire board state from the engine
            # or track all moves perfectly. For this task, we visualize the starting position
            # and allow sending moves.

            self.screen.fill((0, 0, 0))
            self.draw_board()
            pygame.display.flip()
            clock.tick(60)

if __name__ == "__main__":
    gui = GUI()
    gui.run()
