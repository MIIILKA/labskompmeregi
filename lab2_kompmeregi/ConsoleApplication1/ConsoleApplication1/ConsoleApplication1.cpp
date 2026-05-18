#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <fcntl.h>
#include <io.h>

using namespace std;

struct ThreadParams {
    int start, end;
    int id;
    int syncType;
    HANDLE hSemaphore;
    HANDLE hEvent;
    CRITICAL_SECTION* pCS;
    int* pSharedMem;
};

CRITICAL_SECTION consoleCS;

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    ThreadParams* p = (ThreadParams*)lpParam;

    WaitForSingleObject(p->hSemaphore, INFINITE);

    if (p->syncType == 3) {
        EnterCriticalSection(p->pCS);
    }

    for (int i = p->start; (p->start > 0) ? (i <= p->end) : (i >= p->end); (p->start > 0) ? i++ : i--) {

        if (p->syncType == 2) {
            WaitForSingleObject(p->hEvent, INFINITE);
        }

        *(p->pSharedMem) = i;

        EnterCriticalSection(&consoleCS);
        int indent = (p->syncType - 1) * 30;
        wcout << L"\r";
        for (int s = 0; s < indent; s++) wcout << L" ";
        wcout << L"Потік " << p->id << L": " << setw(4) << i << endl;
        LeaveCriticalSection(&consoleCS);

        if (p->syncType == 1) Sleep(20);
        else if (p->syncType == 2) {
            SetEvent(p->hEvent);
            Sleep(40);
        }
        else if (p->syncType == 3) Sleep(40);
    }

    if (p->syncType == 3) {
        LeaveCriticalSection(p->pCS);
    }

    ReleaseSemaphore(p->hSemaphore, 1, NULL);
    return 0;
}

void PrintSeparator(const wstring& message) {
    EnterCriticalSection(&consoleCS);
    wcout << wstring(90, L'=') << endl;
    wcout << L">>> " << message << endl;
    wcout << wstring(90, L'=') << endl;
    LeaveCriticalSection(&consoleCS);
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    wcout << L"ЛАБОРАТОРНА РОБОТА №2. СИНХРОНІЗАЦІЯ ПОТОКІВ" << endl;
    wcout << wstring(90, L'-') << endl;
    wcout << left << setw(30) << L"БЕЗ СИНХРОНІЗАЦІЇ" << setw(30) << L"ЧЕРЕЗ EVENT" << setw(30) << L"CRITICAL SECTION" << endl;
    wcout << wstring(90, L'-') << endl;

    InitializeCriticalSection(&consoleCS);
    CRITICAL_SECTION logicCS;
    InitializeCriticalSection(&logicCS);

    HANDLE hSem = CreateSemaphoreA(NULL, 2, 2, NULL);
    HANDLE hEv = CreateEventA(NULL, FALSE, TRUE, NULL);

    int sharedVal = 0;
    HANDLE hThreads[6];
    ThreadParams params[6];

    for (int i = 0; i < 6; i++) {
        params[i].id = i + 1;
        params[i].syncType = (i / 2) + 1;
        params[i].hSemaphore = hSem;
        params[i].hEvent = hEv;
        params[i].pCS = &logicCS;
        params[i].pSharedMem = &sharedVal;

        if (i % 2 == 0) { params[i].start = 1; params[i].end = 500; }
        else { params[i].start = -1; params[i].end = -500; }

        hThreads[i] = CreateThread(NULL, 0, WorkerThread, &params[i], CREATE_SUSPENDED, NULL);
    }

    ResumeThread(hThreads[0]);
    ResumeThread(hThreads[1]);
    WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);
    PrintSeparator(L"Метод БЕЗ СИНХРОНІЗАЦІЇ завершено.");
    Sleep(500);

    ResumeThread(hThreads[2]);
    ResumeThread(hThreads[3]);
    WaitForMultipleObjects(2, &hThreads[2], TRUE, INFINITE);
    PrintSeparator(L"Метод ЧЕРЕЗ EVENT завершено.");
    Sleep(500);

    ResumeThread(hThreads[5]);
    Sleep(50);
    ResumeThread(hThreads[4]);
    WaitForMultipleObjects(2, &hThreads[4], TRUE, INFINITE);
    PrintSeparator(L"Метод CRITICAL SECTION завершено.");

    wcout << endl << L"ПРОГРАМУ ЗАВЕРШЕНО УСПІШНО." << endl;

    for (int i = 0; i < 6; i++) CloseHandle(hThreads[i]);
    CloseHandle(hSem);
    CloseHandle(hEv);
    DeleteCriticalSection(&logicCS);
    DeleteCriticalSection(&consoleCS);

    system("pause");
    return 0;
}
