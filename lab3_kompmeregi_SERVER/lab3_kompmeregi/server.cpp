#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define PIPE_NAME L"\\\\.\\pipe\\lab3_pipe"
#define BUFFER_SIZE 512

// Глобальні змінні для керування клієнтами
std::vector<HANDLE> clientPipes;
CRITICAL_SECTION pipesCS;

// Функція розсилки повідомлення всім активним клієнтам
void BroadcastMessage(const char* message) {
    EnterCriticalSection(&pipesCS);
    for (HANDLE hPipe : clientPipes) {
        DWORD bytesWritten;
        WriteFile(hPipe, message, (DWORD)strlen(message), &bytesWritten, NULL);
    }
    LeaveCriticalSection(&pipesCS);
}

// Потік обслуговування конкретного клієнта
DWORD WINAPI ClientHandler(LPVOID lpParam) {
    HANDLE hPipe = (HANDLE)lpParam;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;

    // Реєструємо нового клієнта
    EnterCriticalSection(&pipesCS);
    clientPipes.push_back(hPipe);
    LeaveCriticalSection(&pipesCS);

    std::cout << "[Server] New client joined the group." << std::endl;

    // Цикл читання повідомлень від цього клієнта
    while (ReadFile(hPipe, buffer, BUFFER_SIZE - 1, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        std::cout << "[Log]: " << buffer << std::endl;

        // Пересилаємо отримане повідомлення всім
        BroadcastMessage(buffer);
    }

    // Видаляємо клієнта при відключенні
    EnterCriticalSection(&pipesCS);
    clientPipes.erase(std::remove(clientPipes.begin(), clientPipes.end(), hPipe), clientPipes.end());
    LeaveCriticalSection(&pipesCS);

    std::cout << "[Server] A client has left." << std::endl;
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}

// Потік для виявлення сервера через Mailslot
DWORD WINAPI DiscoveryThread(LPVOID lpParam) {
    HANDLE hSlot = CreateMailslot(SLOT_NAME, 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (hSlot == INVALID_HANDLE_VALUE) return 1;

    std::cout << "[Server] Discovery Mailslot is active." << std::endl;

    while (true) {
        char buffer[BUFFER_SIZE];
        DWORD bytesRead;
        if (ReadFile(hSlot, buffer, BUFFER_SIZE, &bytesRead, NULL)) {
            // Клієнт нас знайшов
        }
    }
    return 0;
}

int main() {
    setlocale(LC_ALL, "Ukrainian");
    InitializeCriticalSection(&pipesCS);

    CreateThread(NULL, 0, DiscoveryThread, NULL, 0, NULL);
    std::cout << "[Server] Group Chat Server started..." << std::endl;

    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) break;

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            CreateThread(NULL, 0, ClientHandler, (LPVOID)hPipe, 0, NULL);
        }
        else {
            CloseHandle(hPipe);
        }
    }

    DeleteCriticalSection(&pipesCS);
    return 0;
}