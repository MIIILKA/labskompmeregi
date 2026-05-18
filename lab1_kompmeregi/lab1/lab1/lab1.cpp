#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <clocale>

using namespace std;

// Допоміжна функція для запуску процесів (виправляє помилку Код 2)
void CreateChildProcess(string args, HANDLE hToInherit = NULL) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    char szFileName[MAX_PATH];
    GetModuleFileNameA(NULL, szFileName, MAX_PATH);

    // Обов'язково беремо шлях у лапки для коректної роботи з пробілами
    string cmd = "\"" + string(szFileName) + "\" " + args;

    // Створюємо змінний буфер, оскільки CreateProcessA може його модифікувати
    vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    // Якщо передано хендл для спадковості, inherit = TRUE
    BOOL inherit = (hToInherit != NULL) ? TRUE : FALSE;

    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, inherit, 0, NULL, NULL, &si, &pi)) {
        cout << "[Помилка] CreateProcess не зміг запустити файл. Код: " << GetLastError() << endl;
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "Ukrainian");
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    // 1. ПЕРЕВІРКА НА ЄДИНИЙ ЕКЗЕМПЛЯР (Іменований м'ютекс)
    HANDLE hSingleInstance = CreateMutexA(NULL, FALSE, "MyUniqueApp_Mykola_Global");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Програма вже запущена - тихо виходимо
        return 0;
    }

    // --- ЛОГІКА ДОЧІРНІХ ПРОЦЕСІВ ---
    if (argc > 1) {
        string mode = argv[1];

        // Режим для анонімного м'ютекса (спадковість)
        if (mode == "child_mutex") {
            HANDLE hInheritedMutex = (HANDLE)stoll(argv[2]);
            if (WaitForSingleObject(hInheritedMutex, 5000) == WAIT_OBJECT_0) {
                cout << "  [Нащадок] Отримано успадкований анонімний м'ютекс." << endl;
                Sleep(2000);
                ReleaseMutex(hInheritedMutex);
            }
            return 0;
        }

        // Режим воркера (семафор)
        if (mode == "worker") {
            int id = stoi(argv[2]);
            HANDLE hSem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "MySem_Mykola");
            if (hSem) {
                WaitForSingleObject(hSem, INFINITE);
                cout << "  [Процес " << id << "] Розпочав роботу (Семафор зайнято)..." << endl;
                Sleep(2500); // Симулюємо роботу
                cout << "  [Процес " << id << "] Завершив роботу (Семафор звільнено)." << endl;
                ReleaseSemaphore(hSem, 1, NULL);
                CloseHandle(hSem);
            }
            return 0;
        }
    }

    // --- ЛОГІКА ГОЛОВНОГО ПРОЦЕСУ ---
    cout << "=== ГОЛОВНИЙ ПРОЦЕС (Mykola) ЗАПУЩЕНО ===" << endl;

    // 2. ПЕРЕДАЧА АНОНІМНОГО М'ЮТЕКСА ПО СПАДКОVOСТІ
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; // bInheritHandle = TRUE
    HANDLE hAnonMutex = CreateMutexA(&sa, FALSE, NULL);
    CreateChildProcess("child_mutex " + to_string((long long)hAnonMutex), hAnonMutex);

    // 3. Створення СЕМАФОРА (макс. 3 одночасно)
    HANDLE hSemaphore = CreateSemaphoreA(NULL, 3, 3, "MySem_Mykola");
    vector<HANDLE> workers;

    // 4. ЗАПУСК 10 ПРОЦЕСІВ
    for (int i = 1; i <= 10; i++) {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char szFileName[MAX_PATH];
        GetModuleFileNameA(NULL, szFileName, MAX_PATH);

        string cmd = "\"" + string(szFileName) + "\" worker " + to_string(i);
        vector<char> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back('\0');

        if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            workers.push_back(pi.hProcess); // Зберігаємо хендл для перевірки стану
            CloseHandle(pi.hThread);
        }
    }

    // 5. ВИТРИМКА ТАЙМЕРА (5 секунд)
    HANDLE hTimer = CreateWaitableTimerA(NULL, TRUE, NULL);
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -50000000LL; // Від'ємне значення = відносний час (50 млн * 100 нс = 5 сек)
    SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);

    cout << "\n[Timer] Очікування 5 секунд..." << endl;
    WaitForSingleObject(hTimer, INFINITE);

    // 6. ПЕРЕВІРКА СТАНУ ПРОЦЕСІВ
    cout << "\n--- СТАН ПРОЦЕСІВ ПІСЛЯ ТАЙМЕРА ---" << endl;
    for (int i = 0; i < (int)workers.size(); i++) {
        // Перевіряємо, чи перейшов об'єкт процесу в сигнальний стан
        DWORD status = WaitForSingleObject(workers[i], 0);
        if (status == WAIT_OBJECT_0) {
            cout << "Процес " << i + 1 << ": ЗАВЕРШЕНО (Сигнальний стан)" << endl;
        }
        else {
            cout << "Процес " << i + 1 << ": ЩЕ ПРАЦЮЄ" << endl;
        }
        CloseHandle(workers[i]);
    }

    // Очищення
    CloseHandle(hAnonMutex);
    if (hSemaphore) CloseHandle(hSemaphore);
    CloseHandle(hTimer);
    CloseHandle(hSingleInstance);

    cout << "\nРоботу завершено." << endl;
    system("pause");
    return 0;
}