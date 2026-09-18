#pragma once
#include "models/models.hpp"
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

struct sqlite3;
namespace tsto {
class Database {
public:
    explicit Database(const std::filesystem::path& path); ~Database();
    Database(const Database&) = delete; Database& operator=(const Database&) = delete;
    void migrate();
    std::optional<Account> find_account_by_email(const std::string&) const;
    std::optional<Account> find_account_by_id(std::int64_t) const;
    std::optional<Account> create_account(const Account&);
    bool add_device(const Device&); bool device_belongs_to(std::int64_t, const std::string&) const;
    bool create_session(const Session&); std::optional<Session> find_session(const std::string&) const; void delete_expired_sessions();
    std::optional<Town> get_town(std::int64_t) const;
    bool save_town(const Town& expected, const std::string& backup_payload);
private: sqlite3* db_{}; mutable std::mutex mutex_; void exec(const std::string&) const;
};
}
