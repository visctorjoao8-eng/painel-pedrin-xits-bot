#pragma once
#ifndef AUTH_H
#define AUTH_H

#include <string>

// Declarações das funções
void ShowMessageBoxError(const std::string& title, const std::string& message);
void ShowMessageBox(const std::string& title, const std::string& message);

// Outras declarações de funções (caso necessário)
// bool CheckAppStatus(const std::string& appID, const std::string& appDataBase, std::string& response);
// bool LoginWithKey(const std::string& key, const std::string& appID, const std::string& appDataBase, std::string& response);
// bool LoginWithUser(const std::string& username, const std::string& password, const std::string& appID, const std::string& appDataBase, std::string& response);
// bool RegisterUser(const std::string& username, const std::string& password, const std::string& key, const std::string& appID, const std::string& appDataBase, std::string& response);

#endif

