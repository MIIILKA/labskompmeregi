#include <windows.h>
#include <iostream>
#include <string>

#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define PIPE_NAME L"\\\\.\\pipe\\lab3_pipe"
#define BUFFER_SIZE 512

DWORD WINAPI ClientHandler(LPVOID hPipe) {
    HANDLE pipe = (HANDLE)hPipe;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead, bytesWritten;

    std::cout << "[Server] Client connected via Pipe!" << std::endl;

    while (ReadFile(pipe, buffer, BUFFER_SIZE - 1, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        std::cout << "[Forum Message]: " << buffer << std::endl;

        const char* response = "Message received";
        // Виправлено: явне приведення до DWORD
        WriteFile(pipe, response, (DWORD)strlen(response), &bytesWritten, NULL);
    }

    std::cout << "[Server] Client disconnected." << std::endl;
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

DWORD WINAPI DiscoveryThread(LPVOID lpParam) {
    HANDLE hSlot = CreateMailslot(SLOT_NAME, 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (hSlot == INVALID_HANDLE_VALUE) return 1;

    std::cout << "[Server] Mailslot created. Waiting for discovery requests..." << std::endl;

    while (true) {
        char buffer[BUFFER_SIZE];
        DWORD bytesRead;
        if (ReadFile(hSlot, buffer, BUFFER_SIZE, &bytesRead, NULL)) {
            std::cout << "[Server] Discovery request received!" << std::endl;
        }
        Sleep(100);
    }
    return 0;
}

int main() {
    CreateThread(NULL, 0, DiscoveryThread, NULL, 0, NULL);

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
    return 0;
}