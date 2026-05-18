#include <windows.h>
#include <iostream>
#include <string>

// Використовуємо крапку для локальної машини
#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define PIPE_NAME L"\\\\.\\pipe\\lab3_pipe"

int main() {
    // 1. Пошук сервера через Mailslot
    HANDLE hSlot = CreateFile(SLOT_NAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hSlot == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Server not found (Mailslot). Run Server first!" << std::endl;
        system("pause");
        return 1;
    }

    const char* discoveryMsg = "CONNECT_REQUEST";
    DWORD bytesWritten;
    // Явне приведення (DWORD) прибирає попередження про втрату даних
    WriteFile(hSlot, discoveryMsg, (DWORD)strlen(discoveryMsg), &bytesWritten, NULL);
    CloseHandle(hSlot);

    std::cout << "Discovery signal sent. Connecting to forum..." << std::endl;
    Sleep(500);

    // 2. Підключення до іменованого каналу (Pipe)
    HANDLE hPipe = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Named Pipe connection failed!" << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "Connected! Type your message (or 'exit'):" << std::endl;

    std::string message;
    while (true) {
        std::cout << "YOU: ";
        std::getline(std::cin, message);
        if (message == "exit") break;

        // Приведення до (DWORD) для message.length()
        WriteFile(hPipe, message.c_str(), (DWORD)message.length(), &bytesWritten, NULL);

        char buffer[512];
        DWORD bytesRead;
        if (ReadFile(hPipe, buffer, 511, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            std::cout << "SERVER: " << buffer << std::endl;
        }
    }

    CloseHandle(hPipe);
    return 0;
}