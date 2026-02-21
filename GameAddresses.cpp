module;

#include <windows.h>
#include <tlhelp32.h>
// #include <iostream>
#include <string>

export module GameAddresses;

const std::string FAILED_OPEN_PROCESS          = "Failed to open process for memory reading\n";
const std::string COULD_NOT_RESOLVE_MODULES    = "Could not resolve required module base addresses\n";

struct WindowData { DWORD pid; HWND hwnd; };

export struct GameAddresses_s {

    uintptr_t   baseAddr      = NULL;
    DWORD       baseSize      = 0;

    uintptr_t   xrNetServer   = NULL;
    uintptr_t   xrGame        = NULL;
    uintptr_t   xrCore        = NULL;
    HANDLE      hProcess      = nullptr;

    // New: pointer size (in bytes) of the target process (4 for 32-bit, 8 for 64-bit)
    unsigned    ptrSize       = sizeof(void*);

} gameAddresses;

static std::wstring toWide(const char* str)
{
    if (!str) return {};

    int sizeNeeded = MultiByteToWideChar(
        CP_UTF8,
        0,
        str,
        -1,
        nullptr,
        0
    );

    std::wstring wide(sizeNeeded, 0);

    MultiByteToWideChar(
        CP_UTF8,
        0,
        str,
        -1,
        wide.data(),
        sizeNeeded
    );

    return wide;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {

    DWORD windowPid;
    GetWindowThreadProcessId(hwnd, &windowPid);
    auto data = reinterpret_cast<WindowData*>(lParam);

    if (windowPid == data->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {

        data->hwnd = hwnd;
        return FALSE;

    }
    return TRUE;
}


bool GetModuleInfo(DWORD pid, const char* moduleName,
                   uintptr_t& baseAddr,
                   DWORD& moduleSize)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        pid
    );

    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);

    std::wstring wModuleName = toWide(moduleName);

    bool found = false;

    if (Module32First(snapshot, &entry)) {

        do {

            if (_wcsicmp(entry.szModule, wModuleName.c_str()) == 0) {

                baseAddr   = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                moduleSize = entry.modBaseSize;
                found = true;
                break;

            }

        } while (Module32Next(snapshot, &entry));

    }

    CloseHandle(snapshot);
    return found;

}

HWND FindGameWindow(const char* processName) {

    DWORD pid       = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    std::wstring wProcessName = toWide(processName);

    if (snapshot != INVALID_HANDLE_VALUE) {

        PROCESSENTRY32 entry;
        entry.dwSize = sizeof(entry);

        if (Process32First(snapshot, &entry)) {

            do {

                if (_wcsicmp(entry.szExeFile, wProcessName.c_str()) == 0) {

                    pid = entry.th32ProcessID;
                    break;

                }

            } while (Process32Next(snapshot, &entry));

        }
        CloseHandle(snapshot);
    }

    if (pid == 0) {

        // std::cout << "Process " << processName << " not found\n";
        return nullptr;

    }

    WindowData data = { pid, nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

    // if (data.hwnd == nullptr) { std::cout << "No visible window found for process " << processName << "\n"; }

    return data.hwnd;
}


DWORD GetProcessIdFromWindow(HWND hwnd) {

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;

}

export void setupGameAddresses () {

    DWORD dummySize;

    HWND hwnd = FindGameWindow("XR_3DA.exe");

    if (!hwnd) {

        // std::cout << "Game process (XR_3DA.exe) or window not found" << std::endl;
        return;

    }

    DWORD pid = GetProcessIdFromWindow(hwnd);

    gameAddresses.hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);

    if (!gameAddresses.hProcess) {

        // std::cout << FAILED_OPEN_PROCESS << std::endl;
        return;

    }

    // detect if target process is running under WOW64 (32-bit target on 64-bit host).
    // If so, use 4-byte pointers when reading remote memory, otherwise keep host pointer size.
    BOOL isWow64 = FALSE;
    if (IsWow64Process(gameAddresses.hProcess, &isWow64)) {
        if (isWow64) {
            gameAddresses.ptrSize = 4;
        } else {
            gameAddresses.ptrSize = sizeof(void*);
        }
    } else {
        // If detection failed, default to host pointer size (safe fallback)
        gameAddresses.ptrSize = sizeof(void*);
    }


    GetModuleInfo(pid, "XR_3DA.exe",gameAddresses.baseAddr, gameAddresses.baseSize);
    GetModuleInfo(pid, "xrNetServer.dll",gameAddresses.xrNetServer,dummySize);
    GetModuleInfo(pid, "xrGame.dll",gameAddresses.xrGame, dummySize);
    GetModuleInfo(pid, "xrCore.dll",gameAddresses.xrCore,dummySize);

    if (!gameAddresses.baseAddr || !gameAddresses.xrNetServer ||
        !gameAddresses.xrGame   || !gameAddresses.xrCore) {

        // std::cout << COULD_NOT_RESOLVE_MODULES << std::endl;
        CloseHandle(gameAddresses.hProcess);
        gameAddresses.hProcess = nullptr;

    }

}

export bool isGameReady() {
    return gameAddresses.hProcess &&
           gameAddresses.baseAddr &&
           gameAddresses.xrCore &&
           gameAddresses.xrGame &&
           gameAddresses.xrNetServer;
}