#pragma execution_character_set("utf-8")
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <windows.h>

using namespace std;


const int MAX_DEGREE = 10000;
const int MAX_THREADS = 32;

static double g_coeffs[MAX_DEGREE + 1];
static int    g_degree;
static double g_x;
static int    g_nthreads;
static double g_partial[MAX_THREADS];
static int    g_chunk_start[MAX_THREADS];
static int    g_chunk_end[MAX_THREADS];

mutex start_mtx;
condition_variable start_cv;
bool start_ready = false;

mutex finish_mtx;
int finished_count = 0;
condition_variable finish_cv;

mutex cout_mtx; 


void init_polynomial(int n) {
    srand(static_cast<unsigned int>(time(nullptr)));
    cout << "\n[Згенеровані коефіцієнти a[i]]:" << endl << "  ";
    for (int i = 0; i <= n; i++) {
        g_coeffs[i] = static_cast<double>(rand() % 21) - 10.0;
        cout << "a[" << i << "]=" << g_coeffs[i] << "  ";
        if ((i + 1) % 5 == 0) cout << "\n  ";
    }
    if (g_coeffs[n] == 0.0) g_coeffs[n] = 1.0;
    cout << endl;
}

double horner_reference() {
    double p = g_coeffs[g_degree];
    for (int i = g_degree - 1; i >= 0; i--)
        p = p * g_x + g_coeffs[i];
    return p;
}

void divide_work() {
    int total = g_degree + 1;
    int base = total / g_nthreads;
    int extra = total % g_nthreads;
    int cur = 0;
    for (int i = 0; i < g_nthreads; i++) {
        g_chunk_start[i] = cur;
        int sz = base + (i < extra ? 1 : 0);
        g_chunk_end[i] = cur + sz - 1;
        cur += sz;
    }
}

// Функція потоку-виконавця
void worker_thread(int id) {
    {
        unique_lock<mutex> lock(start_mtx);
        start_cv.wait(lock, [] { return start_ready; });
    }

    int s = g_chunk_start[id];
    int e = g_chunk_end[id];

    // Обчислення за схемою Горнера для частини
    double partial = g_coeffs[e];
    for (int i = e - 1; i >= s; i--)
        partial = partial * g_x + g_coeffs[i];

    g_partial[id] = partial;

    {
        lock_guard<mutex> lock(cout_mtx);
        cout << "  [Потік " << id << "] Обчислив частину a[" << s << ".." << e << "]: " << partial << endl;

        lock_guard<mutex> lock_finish(finish_mtx);
        finished_count++;
        if (finished_count == g_nthreads) {
            finish_cv.notify_one();
        }
    }
}

 // Об'єднання результатів 
double combine_results() {
    cout << "\n[Крок Master-потоку]: Об'єднання за формулою:" << endl;
    double result = g_partial[g_nthreads - 1];
    cout << "  Початковий блок (останній потік): " << result << endl;

    for (int i = g_nthreads - 2; i >= 0; i--) {
        int sz = g_chunk_end[i] - g_chunk_start[i] + 1;
        double old_val = result;
        result = result * pow(g_x, sz) + g_partial[i];
        cout << "  Склеювання: (" << old_val << " * " << g_x << "^" << sz << ") + " << g_partial[i] << " = " << result << endl;
    }
    return result;
}

 
int main() {
    SetConsoleOutputCP(65001);

    cout << "Крок 1: Введіть параметри" << endl;
    cout << "  Введіть ступінь многочлена (n): "; cin >> g_degree;
    cout << "  Введіть значення x: "; cin >> g_x;
    cout << "  Введіть кількість потоків: "; cin >> g_nthreads;

    if (g_degree < 1) g_degree = 1;
    if (g_nthreads < 1) g_nthreads = 1;
    if (g_nthreads > MAX_THREADS) g_nthreads = MAX_THREADS;
    if (g_nthreads > g_degree + 1) g_nthreads = g_degree + 1;

    init_polynomial(g_degree);
    divide_work();

    vector<thread> workers;
    for (int i = 0; i < g_nthreads; i++) {
        workers.emplace_back(worker_thread, i);
    }

    cout << "\nКрок 2: Головний потік дає команду СТАРТ..." << endl;
    auto t_start = chrono::high_resolution_clock::now();

    {
        lock_guard<mutex> lock(start_mtx);
        start_ready = true;
    }
    start_cv.notify_all();

    {
        unique_lock<mutex> lock(finish_mtx);
        finish_cv.wait(lock, [] { return finished_count == g_nthreads; });
    }
    auto t_end = chrono::high_resolution_clock::now();
    cout << "Крок 3: Усі потоки відзвітували про ФІНІШ." << endl;

    double par_result = combine_results();
    double ref_result = horner_reference();
    chrono::duration<double, milli> elapsed = t_end - t_start;

    cout << "\n ФІНАЛЬНІ РЕЗУЛЬТАТИ"  << endl;
    cout << "  Багатопотоково:  " << fixed << setprecision(8) << par_result << endl;
    cout << "  Еталон (1 потік): " << ref_result << endl;
    cout << "  Час виконання:    " << elapsed.count() << " мс" << endl;
    cout << "_________________" << endl;

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    cout << "\nРоботу завершено. Натисніть Enter, щоб вийти...";
    cin.ignore(); cin.get();

    return 0;
}