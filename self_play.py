import subprocess
import time

def run_self_play(games=1):
    for i in range(games):
        print(f"Starting Game {i+1}...")
        process = subprocess.Popen(
            ['./chess4', '--proto'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

        process.stdin.write("proto\n")
        process.stdin.flush()

        moves = []
        while True:
            process.stdin.write("go\n")
            process.stdin.flush()

            bestmove = None
            while True:
                line = process.stdout.readline()
                if not line: break
                # print(f"DEBUG: {line.strip()}")
                if "BESTMOVE" in line:
                    parts = line.split()
                    idx = parts.index("BESTMOVE")
                    bestmove = parts[idx+1]
                    break
                if "gameover" in line or "draw" in line:
                    print(f"End condition reached: {line.strip()}")
                    break

            if not bestmove:
                print("Game ended or error.")
                break

            moves.append(bestmove)
            # print(f"Move {len(moves)}: {bestmove}")

            if len(moves) > 200: # limit game length
                print("Game reached move limit.")
                break

        process.stdin.write("quit\n")
        process.kill()
        print(f"Game {i+1} finished. Total moves: {len(moves)}")

if __name__ == "__main__":
    run_self_play(2)
