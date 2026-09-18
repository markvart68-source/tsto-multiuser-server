#pragma once
#include "database/database.hpp"
#include "config/config.hpp"
#include <string>
namespace tsto {
struct AuthResult { Account account; Session session; };
class AuthService { public: AuthService(Database&, const Config&); AuthResult register_account(std::string email,std::string password,std::string display_name,std::string device_id); AuthResult login(std::string email,std::string password,std::string device_id); Session authenticate(const std::string& bearer) const; private: Database& db_; Config config_; };
}
