#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>


#define PIPE_IN   L"\\\\.\\pipe\\lab3_pipe_in"
#define PIPE_OUT  L"\\\\.\\pipe\\lab3_pipe_out"
#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define BUFFER_SIZE 4096

CRITICAL_SECTION coutCS;
CRITICAL_SECTION clientsCS;

void Log(const std::string& msg) {
    EnterCriticalSection(&coutCS);
    std::cout << msg << std::endl;
    LeaveCriticalSection(&coutCS);
}

struct Client {
    HANDLE hRead;   
    HANDLE hWrite; 
    std::string nick;
};

std::vector<Client*> clients;

bool SendTo(HANDLE h, const std::string& msg) {
    DWORD len = (DWORD)msg.size(), bw;
    if (!WriteFile(h, &len, 4, &bw, NULL)) return false;
    if (!WriteFile(h, msg.c_str(), len, &bw, NULL)) return false;
    FlushFileBuffers(h);
    return true;
}

bool RecvFrom(HANDLE h, std::string& out) {
    DWORD len = 0, br, got = 0;
    char* p = (char*)&len;
    while (got < 4) {
        br = 0;
        if (!ReadFile(h, p + got, 4 - got, &br, NULL) || br == 0) return false;
        got += br;
    }
    if (len == 0 || len > BUFFER_SIZE) return false;
    out.resize(len);
    got = 0;
    while (got < len) {
        br = 0;
        if (!ReadFile(h, &out[got], len - got, &br, NULL) || br == 0) return false;
        got += br;
    }
    return true;
}

void Broadcast(const std::string& msg) {
    EnterCriticalSection(&clientsCS);
    Log("[Broadcast] \"" + msg + "\" -> " + std::to_string(clients.size()) + " clients");
    for (Client* c : clients)
        SendTo(c->hWrite, msg);
    LeaveCriticalSection(&clientsCS);
}

DWORD WINAPI ClientHandler(LPVOID lpParam) {
    Client* self = (Client*)lpParam;

    // нікнейм
    if (!RecvFrom(self->hRead, self->nick) || self->nick.empty()) {
        CloseHandle(self->hRead); CloseHandle(self->hWrite);
        delete self; return 0;
    }

    EnterCriticalSection(&clientsCS);
    clients.push_back(self);
    LeaveCriticalSection(&clientsCS);

    std::string joinMsg = "[Server] " + self->nick + " joined the chat!";
    Log(joinMsg);
    Broadcast(joinMsg);

    // тільки читання
    while (true) {
        std::string msg;
        if (!RecvFrom(self->hRead, msg)) break;
        Log("MSG: " + msg);
        Broadcast(msg);
    }

    // відключення
    EnterCriticalSection(&clientsCS);
    clients.erase(std::remove(clients.begin(), clients.end(), self), clients.end());
    LeaveCriticalSection(&clientsCS);

    std::string byeMsg = "[Server] " + self->nick + " left the chat.";
    Log(byeMsg);
    Broadcast(byeMsg);

    CloseHandle(self->hRead);
    CloseHandle(self->hWrite);
    delete self;
    return 0;
}

// підключення пари pipe для одного клієнта
DWORD WINAPI AcceptThread(LPVOID) {
    while (true) {
        HANDLE hIn = CreateNamedPipe(PIPE_IN,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

        HANDLE hOut = CreateNamedPipe(PIPE_OUT,
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

        if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) {
            Log("[Server] CreateNamedPipe failed!");
            if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
            if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
            Sleep(100); continue;
        }

        Log("[Server] Waiting for client...");

        // клієнт підключається до обох pipe
        BOOL inOk = ConnectNamedPipe(hIn, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
        BOOL outOk = ConnectNamedPipe(hOut, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;

        if (inOk && outOk) {
            Log("[Server] Client connected.");
            Client* c = new Client();
            c->hRead = hIn;
            c->hWrite = hOut;
            CreateThread(NULL, 0, ClientHandler, c, 0, NULL);
        }
        else {
            CloseHandle(hIn); CloseHandle(hOut);
        }
    }
    return 0;
}

DWORD WINAPI MailslotThread(LPVOID) {
    HANDLE hSlot = CreateMailslot(SLOT_NAME, 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (hSlot == INVALID_HANDLE_VALUE) return 1;
    Log("[Mailslot] started.");
    while (true) {
        char buf[128]; DWORD r = 0;
        if (ReadFile(hSlot, buf, sizeof(buf), &r, NULL) && r > 0)
            Log("[Mailslot] ping.");
    }
    return 0;
}

int main() {
    InitializeCriticalSection(&coutCS);
    InitializeCriticalSection(&clientsCS);
    CreateThread(NULL, 0, MailslotThread, NULL, 0, NULL);
    CreateThread(NULL, 0, AcceptThread, NULL, 0, NULL);
    Log("--- SERVER STARTED ---");
    WaitForSingleObject(GetCurrentThread(), INFINITE);
    return 0;
}