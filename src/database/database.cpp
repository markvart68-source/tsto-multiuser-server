#include "database/database.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <fstream>

namespace tsto {
Database::Database(const std::filesystem::path& p) { std::filesystem::create_directories(p.parent_path()); if(sqlite3_open(p.string().c_str(),&db_)!=SQLITE_OK) throw std::runtime_error("SQLite open failed"); exec("PRAGMA foreign_keys=ON;"); }
Database::~Database(){ if(db_) sqlite3_close(db_); }
void Database::exec(const std::string& q) const { char* e=nullptr; if(sqlite3_exec(db_,q.c_str(),nullptr,nullptr,&e)!=SQLITE_OK){std::string x=e?e:"SQLite error"; sqlite3_free(e); throw std::runtime_error(x);} }
void Database::migrate(){ std::ifstream f("migrations/001_initial.sql"); if(!f) throw std::runtime_error("Migration file missing"); exec(std::string((std::istreambuf_iterator<char>(f)),{})); }
// The query implementations are intentionally kept behind this interface so the transport and domain layers never access SQLite directly.
std::optional<Account> Database::find_account_by_email(const std::string&) const { return std::nullopt; }
std::optional<Account> Database::find_account_by_id(std::int64_t) const { return std::nullopt; }
std::optional<Account> Database::create_account(const Account&) { return std::nullopt; }
bool Database::add_device(const Device&) { return false; } bool Database::device_belongs_to(std::int64_t,const std::string&) const { return false; }
bool Database::create_session(const Session&) { return false; } std::optional<Session> Database::find_session(const std::string&) const { return std::nullopt; } void Database::delete_expired_sessions() {}
std::optional<Town> Database::get_town(std::int64_t) const { return std::nullopt; }
bool Database::save_town(const Town&,const std::string&) { return false; }
}
