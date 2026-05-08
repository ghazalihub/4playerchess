import subprocess
import threading
import queue
from flask import Flask, render_template
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app)

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
        self.turn = "red"
        self.board = {}
        self.winner = None

        threading.Thread(target=self._read_stream, daemon=True).start()
        self.send("proto")
        self.send("ready")
        self.sync_board()

    def _read_stream(self):
        for line in iter(self.process.stdout.readline, ''):
            line = line.strip()
            if not line: continue
            print(f"ENGINE: {line}")
            if line == "moveok" or line.startswith("bestmove"):
                self.sync_board()
            elif line.startswith("turn"):
                self.turn = line.split()[1]
                self.broadcast_state()
            elif line.startswith("gameover"):
                self.winner = line.split()[1]
                self.broadcast_state()
            elif line == "board":
                self.board = {}
            elif len(line.split()) == 4:
                parts = line.split()
                try:
                    r, c = int(parts[0]), int(parts[1])
                    color, p_type = parts[2], parts[3]
                    self.board[f"{r},{c}"] = {"color": color, "type": p_type}
                except: pass
            elif line == "boardok":
                self.broadcast_state()

    def send(self, cmd):
        self.process.stdin.write(cmd + '\n')
        self.process.stdin.flush()

    def sync_board(self):
        self.send("board")

    def broadcast_state(self):
        socketio.emit('update', {
            'board': self.board,
            'turn': self.turn,
            'winner': self.winner
        })

engine = ChessEngine()

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('move')
def handle_move(move_str):
    print(f"GUI Move: {move_str}")
    engine.send(move_str)

@socketio.on('request_sync')
def handle_sync():
    engine.broadcast_state()

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
