import random
import math

N = 8

def count_conflicts(board):
    conflicts = 0
    for i in range(N):
        for j in range(i + 1, N):
            if board[i] == board[j] or abs(i - j) == abs(board[i] - board[j]):
                conflicts += 1
    return conflicts

def print_board(board):
    print("\n  0 1 2 3 4 5 6 7")
    for i in range(N):
        line = f"{i} "
        for j in range(N):
            line += "Q " if board[i] == j else ". "
        print(line)
    print(f"Конфліктів: {count_conflicts(board)}")

def simulated_annealing():
    # 1. Початковий хаотичний стан
    current_sol = [random.randint(0, N-1) for _ in range(N)]
    current_err = count_conflicts(current_sol)
    
    best_err = current_err
    
    print(f"Початковий стан: конфліктів {current_err}")
    
    t = 10.0
    alpha = 0.999
    step = 0
    
    while current_err > 0 and t > 0.01:
        step += 1
        row = random.randint(0, N-1)
        old_col = current_sol[row]
        current_sol[row] = random.randint(0, N-1)
        
        next_err = count_conflicts(current_sol)
        diff = next_err - current_err
        
        if next_err < best_err:
            print(f"крок {step:4} конфліктів {next_err}")
            best_err = next_err
            current_err = next_err
        # Логіка відпалу
        elif diff < 0 or random.random() < math.exp(-diff / t):
            current_err = next_err
        else:
            current_sol[row] = old_col 
            
        t *= alpha
        
    return current_sol

def solve_backtracking(board, row):
    if row == N: return True
    for col in range(N):
        if all(board[i] != col and abs(i - row) != abs(board[i] - col) for i in range(row)):
            board[row] = col
            if solve_backtracking(board, row + 1): return True
    return False

# ЗАПУСК
print("--- ЗАПУСК АЛГОРИТМУ ВІДПАЛУ ---")
res = simulated_annealing()
print_board(res)

print("\n--- ЗАПУСК BACKTRACKING (Пункт 8) ---")
bt_res = [-1] * N
solve_backtracking(bt_res, 0)
print_board(bt_res)
