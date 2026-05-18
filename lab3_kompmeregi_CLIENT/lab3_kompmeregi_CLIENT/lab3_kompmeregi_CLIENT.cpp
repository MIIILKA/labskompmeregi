#include <windows.h>
#include <string>
#include <deque>

#define PIPE_IN   L"\\\\.\\pipe\\lab3_pipe_in"   
#define PIPE_OUT  L"\\\\.\\pipe\\lab3_pipe_out"  
#define SLOT_NAME L"\\\\.\\mailslot\\server_discovery"
#define BUFFER_SIZE 4096

HANDLE hWrite = INVALID_HANDLE_VALUE; 
HANDLE hRead = INVALID_HANDLE_VALUE; 
HANDLE hOut;
CRITICAL_SECTION drawCS;
std::deque<std::string> chatLines;
std::string inputLine;
bool running = true;
std::string nick;

bool SendMsg(const std::string& msg) {
    DWORD len = (DWORD)msg.size(), bw;
    if (!WriteFile(hWrite, &len, 4, &bw, NULL)) return false;
    if (!WriteFile(hWrite, msg.c_str(), len, &bw, NULL)) return false;
    FlushFileBuffers(hWrite);
    return true;
}

bool RecvMsg(std::string& out) {
    DWORD len = 0, br, got = 0;
    char* p = (char*)&len;
    while (got < 4) {
        br = 0;
        if (!ReadFile(hRead, p + got, 4 - got, &br, NULL) || br == 0) return false;
        got += br;
    }
    if (len == 0 || len > BUFFER_SIZE) return false;
    out.resize(len);
    got = 0;
    while (got < len) {
        br = 0;
        if (!ReadFile(hRead, &out[got], len - got, &br, NULL) || br == 0) return false;
        got += br;
    }
    return true;
}

// UI 
int GetW() { CONSOLE_SCREEN_BUFFER_INFO i; return GetConsoleScreenBufferInfo(hOut, &i) ? i.srWindow.Right - i.srWindow.Left + 1 : 80; }
int GetH() { CONSOLE_SCREEN_BUFFER_INFO i; return GetConsoleScreenBufferInfo(hOut, &i) ? i.srWindow.Bottom - i.srWindow.Top + 1 : 25; }

void Redraw() {
    EnterCriticalSection(&drawCS);
    int W = GetW(), H = GetH(), chatRows = H - 2;
    int start = (int)chatLines.size() - chatRows;
    if (start < 0) start = 0;
    DWORD w;
    for (int row = 0;row < chatRows;row++) {
        COORD pos = { 0,(SHORT)row };
        SetConsoleCursorPosition(hOut, pos);
        std::string blank(W, ' ');
        WriteConsoleA(hOut, blank.c_str(), W, &w, NULL);
        SetConsoleCursorPosition(hOut, pos);
        int idx = start + row;
        if (idx >= 0 && idx < (int)chatLines.size()) {
            std::string line = chatLines[idx];
            if ((int)line.size() >= W) line = line.substr(0, W - 1);
            WriteConsoleA(hOut, line.c_str(), (DWORD)line.size(), &w, NULL);
        }
    }
    // роздільник
    { COORD pos = { 0,(SHORT)(H - 2) }; SetConsoleCursorPosition(hOut, pos); std::string sep(W, '-'); WriteConsoleA(hOut, sep.c_str(), (DWORD)sep.size(), &w, NULL); }
    // рядок вводу
    { COORD pos = { 0,(SHORT)(H - 1) }; SetConsoleCursorPosition(hOut, pos); std::string blank(W, ' '); WriteConsoleA(hOut, blank.c_str(), W, &w, NULL); SetConsoleCursorPosition(hOut, pos); std::string prompt = "> " + inputLine; if ((int)prompt.size() >= W) prompt = prompt.substr(0, W - 1); WriteConsoleA(hOut, prompt.c_str(), (DWORD)prompt.size(), &w, NULL); }

    { int cx = 2 + (int)inputLine.size(); if (cx >= W) cx = W - 1; COORD cur = { (SHORT)cx,(SHORT)(H - 1) }; SetConsoleCursorPosition(hOut, cur); }
    LeaveCriticalSection(&drawCS);
}

void AddLine(const std::string& msg) {
    EnterCriticalSection(&drawCS);
    int W = GetW(); std::string line = msg;
    while ((int)line.size() >= W) { chatLines.push_back(line.substr(0, W - 1)); line = line.substr(W - 1); }
    chatLines.push_back(line);
    while ((int)chatLines.size() > 1000) chatLines.pop_front();
    LeaveCriticalSection(&drawCS);
    Redraw();
}

DWORD WINAPI Receiver(LPVOID) {
    // цей потік тільк читає 
    while (running) {
        std::string msg;
        if (!RecvMsg(msg)) { if (running) AddLine("[!] Disconnected."); running = false; break; }
        AddLine(msg);
    }
    return 0;
}

HANDLE ConnectPipe(const wchar_t* name, DWORD access) {
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 20 && h == INVALID_HANDLE_VALUE; i++) {
        h = CreateFile(name, access, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_PIPE_BUSY) WaitNamedPipe(name, 2000);
            else Sleep(200);
        }
    }
    return h;
}

int main() {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    InitializeCriticalSection(&drawCS);

    // Mailslot
    { HANDLE hSlot = CreateFile(SLOT_NAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (hSlot != INVALID_HANDLE_VALUE) { DWORD bw; WriteFile(hSlot, "PING", 4, &bw, NULL); CloseHandle(hSlot); } }
    Sleep(200);

    // підключення до двох pipe
    hWrite = ConnectPipe(PIPE_IN, GENERIC_WRITE); // пише в IN
    hRead = ConnectPipe(PIPE_OUT, GENERIC_READ);  // читає з OUT

    if (hWrite == INVALID_HANDLE_VALUE || hRead == INVALID_HANDLE_VALUE) {
        WriteConsoleA(hOut, "Error: Cannot connect to server!\r\n", 33, NULL, NULL);
        Sleep(3000); return 1;
    }

    // нік
    { DWORD w; std::string pr = "Connected! Enter nickname: "; WriteConsoleA(hOut, pr.c_str(), (DWORD)pr.size(), &w, NULL); char buf[64] = {}; DWORD r = 0; ReadConsoleA(hIn, buf, 63, &r, NULL); nick = std::string(buf, r); while (!nick.empty() && (nick.back() == '\n' || nick.back() == '\r')) nick.pop_back(); if (nick.empty()) nick = "Anonymous"; }

    SendMsg(nick);
    Sleep(150);

    { CONSOLE_SCREEN_BUFFER_INFO csbi; GetConsoleScreenBufferInfo(hOut, &csbi); COORD origin = { 0,0 }; DWORD w; FillConsoleOutputCharacterA(hOut, ' ', csbi.dwSize.X * csbi.dwSize.Y, origin, &w); SetConsoleCursorPosition(hOut, origin); }

    AddLine("=== Chat started. Type 'exit' to quit. ===");
    Redraw();
    CreateThread(NULL, 0, Receiver, NULL, 0, NULL);

    DWORD savedMode; GetConsoleMode(hIn, &savedMode);
    SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);

    while (running) {
        INPUT_RECORD rec; DWORD nr = 0;
        if (!ReadConsoleInput(hIn, &rec, 1, &nr) || nr == 0) continue;
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        WCHAR wch = rec.Event.KeyEvent.uChar.UnicodeChar;

        if (vk == VK_RETURN) {
            if (inputLine == "exit") { running = false;break; }
            if (!inputLine.empty()) {
                std::string full = nick + ": " + inputLine;
                inputLine.clear(); Redraw();
                if (!SendMsg(full)) { AddLine("[!] Send failed."); running = false; }
            }
        }
        else if (vk == VK_BACK) {
            if (!inputLine.empty()) { inputLine.pop_back(); Redraw(); }
        }
        else if (wch >= 32) {
            char mb[4] = {}; int len = WideCharToMultiByte(CP_OEMCP, 0, &wch, 1, mb, 4, NULL, NULL);
            if (len > 0 && (int)inputLine.size() < GetW() - 5) { for (int i = 0;i < len;i++) inputLine += mb[i]; Redraw(); }
        }
    }

    SetConsoleMode(hIn, savedMode);
    CloseHandle(hWrite); CloseHandle(hRead);
    DeleteCriticalSection(&drawCS);
    return 0;
}