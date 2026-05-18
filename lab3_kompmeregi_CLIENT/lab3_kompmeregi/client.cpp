#include <windows.h>
#include <iostream>
#include <string>

#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define PIPE_NAME L"\\\\.\\pipe\\lab3_pipe"

// Потік для отримання повідомлень від сервера в реальному часі
DWORD WINAPI ReceiverThread(LPVOID lpParam) {
    HANDLE hPipe = (HANDLE)lpParam;
    char buffer[512];
    DWORD bytesRead;

    while (ReadFile(hPipe, buffer, 511, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        // Виводимо повідомлення від інших
        std::cout << "\r" << std::string(50, ' ') << "\r"; // Очистка рядка вводу
        std::cout << buffer << std::endl;
        std::cout << "> "; // Повертаємо символ вводу
    }
    std::cout << "\n[System] Connection to server lost." << std::endl;
    return 0;
}

int main() {
    setlocale(LC_ALL, "Ukrainian");

    // 1. Пошук сервера через Mailslot
    HANDLE hSlot = CreateFile(SLOT_NAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSlot == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Server not found!" << std::endl;
        system("pause");
        return 1;
    }

    const char* discoveryMsg = "PING";
    DWORD bytesWritten;
    WriteFile(hSlot, discoveryMsg, (DWORD)strlen(discoveryMsg), &bytesWritten, NULL);
    CloseHandle(hSlot);

    Sleep(500);

    // 2. Підключення до Pipe
    HANDLE hPipe = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Pipe connection failed!" << std::endl;
        system("pause");
        return 1;
    }

    std::string nickname;
    std::cout << "=== Welcome to Chat ===\nEnter your nickname: ";
    std::getline(std::cin, nickname);

    // Створюємо потік для прийому повідомлень
    CreateThread(NULL, 0, ReceiverThread, (LPVOID)hPipe, 0, NULL);

    std::string text;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, text);
        if (text == "exit") break;
        if (text.empty()) continue;

        std::string fullMsg = nickname + ": " + text;
        WriteFile(hPipe, fullMsg.c_str(), (DWORD)fullMsg.length(), &bytesWritten, NULL);
    }

    CloseHandle(hPipe);
    return 0;
}