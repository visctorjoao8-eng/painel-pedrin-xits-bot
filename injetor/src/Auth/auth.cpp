#define CURL_STATICLIB
#include <iostream>
#include <string>
#include "curl/curl.h"
#include "json.hpp"  // nlohmann json header file
#include <Windows.h>
#include <thread>  // Adicionando para o uso de threads
#include <wininet.h>  // Para pegar o IP (WinINet API)
#include <atlsecurity.h>
#include <atlstr.h>
#pragma comment(lib, "wininet.lib")
#pragma comment (lib, "src/Auth/curl/libcurl_a.lib")

using json = nlohmann::json;
using namespace std;


// Função para obter o IP público
std::string GetIPInfo() {
    std::string ip = "Unknown";
    HINTERNET hInternet = InternetOpenW(L"GetIP", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetOpenUrlA(hInternet, "http://ip-api.com/json/", NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hConnect) {
            char buffer[1024];
            DWORD bytesRead = 0;
            if (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead)) {
                buffer[bytesRead] = '\0';  // Null-terminate the string
                ip = std::string(buffer);  // Return the raw JSON response (we'll parse it later)
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
    return ip;
}


static std::string get_hwid() {
    ATL::CAccessToken accessToken;
    ATL::CSid currentUserSid;
    if (accessToken.GetProcessToken(TOKEN_READ | TOKEN_QUERY) &&
        accessToken.GetUser(&currentUserSid))
        return std::string(CT2A(currentUserSid.Sid()));
    return "none";
}


// Função para exibir MessageBox com o título e mensagem fornecida
void ShowMessageBoxError(const std::string& title, const std::string& message) {
    int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    wchar_t* wide_message = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wide_message, len);
    MessageBoxW(NULL, wide_message, std::wstring(title.begin(), title.end()).c_str(), MB_OK | MB_ICONERROR);
    delete[] wide_message;
}

void ShowMessageBox(const std::string& title, const std::string& message) {
    int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    wchar_t* wide_message = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wide_message, len);
    MessageBoxW(NULL, wide_message, std::wstring(title.begin(), title.end()).c_str(), MB_OK | MB_ICONINFORMATION);
    delete[] wide_message;
}

// Função para enviar o log para a API
// size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
//     ((std::string*)userp)->append((char*)contents, size * nmemb);
//     return size * nmemb;
// }

// void SendLogToAPI(const std::string& usernameOrKey, const std::string& hwid, const std::string& ipInfo, bool isKeyLogin, const std::string& appid, const std::string& appDatabase) {
//     CURL* curl;
//     CURLcode res;
//     std::string response;

//     char computerUsername[256];
//     size_t len = 0;
//     getenv_s(&len, computerUsername, sizeof(computerUsername), "USERNAME");

//     json logData = {
//         {"usernameOrKey", usernameOrKey},
//         {"hwid", hwid},
//         {"ipInfo", ipInfo},
//         {"isKeyLogin", isKeyLogin},
//         {"computerUsername", std::string(computerUsername)},  // Usando _dupenv_s
//         {"appid", appid},
//         {"appDatabase", appDatabase}
//     };

//     std::string jsonPayload = logData.dump();  // Converter para string JSON

//     curl_global_init(CURL_GLOBAL_DEFAULT);
//     curl = curl_easy_init();

//     if (curl) {
//         curl_easy_setopt(curl, CURLOPT_URL, "https://4uth.squareweb.app/log-login");

//         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());

//         struct curl_slist* headers = NULL;
//         headers = curl_slist_append(headers, "Content-Type: application/json");
//         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

//         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
//         curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

//         // Envia a requisição
//         res = curl_easy_perform(curl);

//         if (res != CURLE_OK) {
//             std::cerr << "Erro ao enviar log para API: " << curl_easy_strerror(res) << std::endl;
//         }

//         curl_easy_cleanup(curl);
//     }

//     curl_global_cleanup();
// }

// Função para realizar a requisição HTTP POST com cURL
// bool SendRequest(const std::string& url, const json& data, std::string& response) {
//     CURL* curl;
//     CURLcode res;

//     curl_global_init(CURL_GLOBAL_DEFAULT);
//     curl = curl_easy_init();

//     if (curl) {
//         std::string json_data = data.dump();  // Convertendo o JSON para string

//         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());

//         // Definindo o cabeçalho content-type como application/json
//         struct curl_slist* headers = NULL;
//         headers = curl_slist_append(headers, "Content-Type: application/json");
//         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

//         // Armazenando a resposta da requisição
//         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
//         curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

//         // Enviando a requisição
//         res = curl_easy_perform(curl);

//         if (res != CURLE_OK) {
//             std::cerr << "cURL request failed: " << curl_easy_strerror(res) << std::endl;
//             curl_easy_cleanup(curl);
//             curl_global_cleanup();
//             return false;
//         }

//         curl_easy_cleanup(curl);
//     }
//     curl_global_cleanup();
//     return true;
// }

// Função para realizar o login com Key
// bool LoginWithKey(const std::string& key, const std::string& appID, const std::string& appDataBase, std::string& response) {
//     std::string hwid = get_hwid();  // Aqui você obtém o HWID do sistema
//     std::string ipInfo = GetIPInfo();  // Obtém o IP usando a função GetIPInfo

//     json loginData = {
//         {"key", key},
//         {"hwid", hwid},
//         {"appid", appID},
//         {"appDatabase", appDataBase}
//     };

//     bool success = SendRequest("https://4uth.squareweb.app/login", loginData, response);

//     if (success) {
//         // Verifica se a resposta contém uma mensagem de sucesso diretamente
//         if (response.find("Login bem-sucedido") != std::string::npos) {
//             // Se a resposta contiver "Login bem-sucedido", envia o log
//             std::thread logThread(SendLogToAPI, key, hwid, ipInfo, true, appID, appDataBase);
//             logThread.detach();  // Desanexa o thread, para que ele rode de forma independente

//         }
//         else {

//         }
//     }

//     return success;
// }



// Função para realizar login com Usuário
// bool LoginWithUser(const std::string& username, const std::string& password, const std::string& appID, const std::string& appDataBase, std::string& response) {
//     std::string hwid = get_hwid();    // Aqui você obtém o HWID do sistema
//     std::string ipInfo = GetIPInfo();  // Obtém o IP usando a função GetIPInfo

//     json userLoginData = {
//         {"username", username},
//         {"password", password},
//         {"hwid", hwid},
//         {"appid", appID},
//         {"appDatabase", appDataBase}
//     };

//     bool success = SendRequest("https://4uth.squareweb.app/user-login", userLoginData, response);

//     if (success) {
//         // Verifica se a resposta contém uma mensagem de sucesso diretamente
//         if (response.find("Login bem-sucedido") != std::string::npos) {
//             // Se a resposta contiver "Login bem-sucedido", envia o log
//             std::thread logThread(SendLogToAPI, username, hwid, ipInfo, false, appID, appDataBase);
//             logThread.detach();  // Desanexa o thread, para que ele rode de forma independente
//         }
//         else {
//             // Caso a resposta não seja de sucesso, exibe a falha no login

//         }
//     }

//     return success;
// }


// Função para registrar um usuário
// bool RegisterUser(const std::string& username, const std::string& password, const std::string& key, const std::string& appID, const std::string& appDataBase, std::string& response) {
//     json registerData = {
//         {"username", username},
//         {"password", password},
//         {"key", key},
//         {"appid", appID},
//         {"appDatabase", appDataBase}
//     };

//     bool success = SendRequest("https://4uth.squareweb.app/register", registerData, response);


//     return success;
// }

// Função para verificar o status do AppID
// bool CheckAppStatus(const std::string& appID, const std::string& appDataBase, std::string& response) {
//     json checkData = {
//         {"appid", appID},
//         {"appDatabase", appDataBase}
//     };
//     return SendRequest("https://4uth.squareweb.app/check-app-status", checkData, response);
// }

