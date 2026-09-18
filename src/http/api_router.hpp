#pragma once
#include "database/database.hpp"
#include "config/config.hpp"
#include <map>
#include <string>

namespace tsto {
struct AuthResult {
    Account account;
    Session session;
    std::string access_token;
};

class AuthService {
public:
    AuthService(Database&, const Config&);
    AuthResult register_account(std::string email, std::string password, std::string display_name, std::string device_id);
    AuthResult login(std::string email, std::string password, std::string device_id);
    void register_device(const Session&, std::string device_id, std::string platform, std::string app_version);
    Session authenticate(const std::string&) const;
    Account account(std::int64_t account_id) const;
    std::map<std::string, std::int64_t> currencies(std::int64_t account_id) const;
    void set_currency(std::int64_t account_id, const std::string& currency, std::int64_t amount);
private:
    Database& db_;
    Config config_;
};
}
