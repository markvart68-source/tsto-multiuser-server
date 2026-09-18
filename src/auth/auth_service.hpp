#pragma once
#include "database/database.hpp"
#include "config/config.hpp"
#include <string>
namespace tsto {
struct AuthResult { Account account; Session session; std::string access_token; };
class AuthService {
public:
    AuthService(Database&, const Config&);
    AuthResult register_account(std::string email, std::string password, std::string display_name, std::string device_id);
    AuthResult login(std::string email, std::string password, std::string device_id);
    void register_device(const Session& session, std::string device_id, std::string platform, std::string app_version);
    Session authenticate(const std::string& authorization) const;
private:
    Database& db_;
    Config config_;
};
}
