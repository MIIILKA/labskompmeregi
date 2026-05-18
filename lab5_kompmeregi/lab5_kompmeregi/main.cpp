#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <string>
#include <io.h>
#include <fcntl.h>
#include <time.h>

// взаємне блокування

std::wstring getTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[16];
    swprintf(buf, 16, L"[%02d:%02d:%02d]", st.wHour, st.wMinute, st.wSecond);
    return std::wstring(buf);
}

void user_log(const std::wstring& msg) {
    std::wcout << getTime() << L" " << msg << std::endl;
}

int main(int argc, char* argv[]) {
    (void)_setmode(_fileno(stdout), _O_U16TEXT);

    const char* mNameA = "Local\\MtxA_Human";
    const char* mNameB = "Local\\MtxB_Human";

    if (argc == 1) {
        SetConsoleTitleW(L"ПАНЕЛЬ КЕРУВАННЯ (Батьківський процес)");

        user_log(L"Створюю ресурси (М'ютекси А та В)...");
        HANDLE hA = CreateMutexA(NULL, FALSE, mNameA);
        HANDLE hB = CreateMutexA(NULL, FALSE, mNameB);

        wchar_t wPath[MAX_PATH];
        GetModuleFileNameW(NULL, wPath, MAX_PATH);

        STARTUPINFOW si1 = { sizeof(si1) }, si2 = { sizeof(si2) };
        PROCESS_INFORMATION pi1 = { 0 }, pi2 = { 0 };
        ZeroMemory(&si1, sizeof(si1)); si1.cb = sizeof(si1);
        ZeroMemory(&si2, sizeof(si2)); si2.cb = sizeof(si2);

        wchar_t cmd1[MAX_PATH + 10], cmd2[MAX_PATH + 10];
        swprintf(cmd1, MAX_PATH + 10, L"\"%s\" 1", wPath);
        swprintf(cmd2, MAX_PATH + 10, L"\"%s\" 2", wPath);

        user_log(L"Запускаю два паралельні процеси...");
        CreateProcessW(NULL, cmd1, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si1, &pi1);
        CreateProcessW(NULL, cmd2, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si2, &pi2);

        user_log(L"Процеси запущені. Очікую на виникнення Deadlock...");

        HANDLE processes[2] = { pi1.hProcess, pi2.hProcess };
        int remaining = 2;

        while (remaining > 0) {
            DWORD wait = WaitForMultipleObjects(remaining, processes, FALSE, INFINITE);
            int index = wait - WAIT_OBJECT_0;
            if (index >= 0 && index < remaining) {
                user_log(L"Один із процесів завершився.");
                CloseHandle(processes[index]);
                for (int i = index; i < remaining - 1; i++) processes[i] = processes[i + 1];
                remaining--;
                if (remaining > 0) user_log(L"Залишився ще один активний процес.");
            }
        }

        user_log(L"Всі процеси завершили роботу. Натисніть ENTER для виходу.");
        if (hA) CloseHandle(hA); if (hB) CloseHandle(hB);
        std::wcin.get();
    }
    else {
        int mode = std::stoi(argv[1]);
        Sleep(800);

        HANDLE hA = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mNameA);
        HANDLE hB = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mNameB);

        if (mode == 1) {
            SetConsoleTitleW(L"Процес №1 (Черга: А -> В)");
            user_log(L"Процес №1: Намагаюся захопити М'ютекс А");
            WaitForSingleObject(hA, INFINITE);
            user_log(L"Процес №1: Успішно захопив М'ютекс А");
            Sleep(3000);
            user_log(L"Процес №1: [ЗАБЛОКОВАНО] - Чекаю, поки Процес №2 віддасть М'ютекс В");
            WaitForSingleObject(hB, INFINITE);
            user_log(L"Процес №1: Отримав М'ютекс В і завершую роботу.");
            ReleaseMutex(hB); ReleaseMutex(hA);
        }
        else {
            SetConsoleTitleW(L"Процес №2 (Черга: В -> А)");
            user_log(L"Процес №2: Намагаюся захопити М'ютекс В");
            WaitForSingleObject(hB, INFINITE);
            user_log(L"Процес №2: Успішно захопив М'ютекс В.");
            Sleep(3000);
            user_log(L"Процес №2: [ЗАБЛОКОВАНО] - Чекаю, поки Процес №1 віддасть М'ютекс А");
            WaitForSingleObject(hA, INFINITE);
            user_log(L"Процес №2: Отримав М'ютекс А і завершую роботу.");
            ReleaseMutex(hA); ReleaseMutex(hB);
        }
        user_log(L"Натисніть ENTER для закриття цього вікна.");
        std::wcin.get();
    }
    return 0;
}