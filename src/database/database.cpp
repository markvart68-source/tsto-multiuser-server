#include "database/database.hpp"
#include <sqlite3.h>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace tsto {
namespace {
void require_sqlite(int rc, sqlite3* db, const char* operation) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
}
std::int64_t unix_now() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
void bind_text(sqlite3_stmt* s, int index, const std::string& value) { require_sqlite(sqlite3_bind_text(s,index,value.c_str(),-1,SQLITE_TRANSIENT),sqlite3_db_handle(s),"bind"); }
std::string column_text(sqlite3_stmt* s, int i) { const auto* p = sqlite3_column_text(s,i); return p ? reinterpret_cast<const char*>(p) : std::string{}; }
}
Database::Database(const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    if (sqlite3_open_v2(path.string().c_str(), &db_, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) throw std::runtime_error("unable to open SQLite database");
    exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;");
}
Database::~Database() { if (db_) sqlite3_close(db_); }
void Database::exec(const std::string& sql) const { char* error=nullptr; if (sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&error)!=SQLITE_OK) { std::string message=error?error:"SQLite error"; sqlite3_free(error); throw std::runtime_error(message); } }
void Database::migrate() {
    std::ifstream file("migrations/001_initial.sql");
    if (!file) throw std::runtime_error("migrations/001_initial.sql not found");
    exec(std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
}
std::optional<Account> Database::find_account_by_email(const std::string& email) const {
    std::lock_guard lock(mutex_); sqlite3_stmt* s=nullptr;
    require_sqlite(sqlite3_prepare_v2(db_,"SELECT id,email,password_hash,display_name,user_id,mayhem_id FROM accounts WHERE email=? COLLATE NOCASE",-1,&s,nullptr),db_,"prepare account lookup"); bind_text(s,1,email);
    std::optional<Account> result; if (sqlite3_step(s)==SQLITE_ROW) result=Account{sqlite3_column_int64(s,0),column_text(s,1),column_text(s,2),column_text(s,3),column_text(s,4),sqlite3_column_int64(s,5)}; sqlite3_finalize(s); return result;
}
std::optional<Account> Database::find_account_by_id(std::int64_t id) const {
    std::lock_guard lock(mutex_); sqlite3_stmt* s=nullptr;
    require_sqlite(sqlite3_prepare_v2(db_,"SELECT id,email,password_hash,display_name,user_id,mayhem_id FROM accounts WHERE id=?",-1,&s,nullptr),db_,"prepare account lookup"); sqlite3_bind_int64(s,1,id);
    std::optional<Account> result; if (sqlite3_step(s)==SQLITE_ROW) result=Account{sqlite3_column_int64(s,0),column_text(s,1),column_text(s,2),column_text(s,3),column_text(s,4),sqlite3_column_int64(s,5)}; sqlite3_finalize(s); return result;
}
std::optional<Account> Database::create_account(const Account& account) {
    std::lock_guard lock(mutex_); sqlite3_stmt* s=nullptr;
    require_sqlite(sqlite3_prepare_v2(db_,"INSERT INTO accounts(email,password_hash,display_name,user_id,mayhem_id) VALUES(?,?,?,?,?)",-1,&s,nullptr),db_,"prepare account insert"); bind_text(s,1,account.email); bind_text(s,2,account.password_hash); bind_text(s,3,account.display_name); bind_text(s,4,account.user_id); sqlite3_bind_int64(s,5,account.mayhem_id);
    if (sqlite3_step(s)!=SQLITE_DONE) { sqlite3_finalize(s); return std::nullopt; } auto id=sqlite3_last_insert_rowid(db_); sqlite3_finalize(s);
    sqlite3_stmt* q=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"SELECT id,email,password_hash,display_name,user_id,mayhem_id FROM accounts WHERE id=?",-1,&q,nullptr),db_,"prepare account readback"); sqlite3_bind_int64(q,1,id); std::optional<Account> result; if(sqlite3_step(q)==SQLITE_ROW) result=Account{sqlite3_column_int64(q,0),column_text(q,1),column_text(q,2),column_text(q,3),column_text(q,4),sqlite3_column_int64(q,5)}; sqlite3_finalize(q); return result;
}
bool Database::add_device(const Device& d) { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"INSERT INTO devices(id,account_id,platform,app_version) VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET account_id=excluded.account_id,platform=excluded.platform,app_version=excluded.app_version,last_seen_at=CURRENT_TIMESTAMP",-1,&s,nullptr),db_,"prepare device upsert"); bind_text(s,1,d.id); sqlite3_bind_int64(s,2,d.account_id); bind_text(s,3,d.platform); bind_text(s,4,d.app_version); bool ok=sqlite3_step(s)==SQLITE_DONE; sqlite3_finalize(s); return ok; }
bool Database::device_belongs_to(std::int64_t account_id,const std::string& device_id) const { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"SELECT 1 FROM devices WHERE id=? AND account_id=?",-1,&s,nullptr),db_,"prepare device lookup"); bind_text(s,1,device_id); sqlite3_bind_int64(s,2,account_id); bool ok=sqlite3_step(s)==SQLITE_ROW; sqlite3_finalize(s); return ok; }
bool Database::create_session(const Session& session) { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"INSERT INTO sessions(token_hash,account_id,device_id,expires_at) VALUES(?,?,?,?)",-1,&s,nullptr),db_,"prepare session insert"); bind_text(s,1,session.token_hash); sqlite3_bind_int64(s,2,session.account_id); bind_text(s,3,session.device_id); bind_text(s,4,session.expires_at); bool ok=sqlite3_step(s)==SQLITE_DONE; sqlite3_finalize(s); return ok; }
std::optional<Session> Database::find_session(const std::string& hash) const { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"SELECT token_hash,account_id,device_id,expires_at FROM sessions WHERE token_hash=? AND CAST(expires_at AS INTEGER)>?",-1,&s,nullptr),db_,"prepare session lookup"); bind_text(s,1,hash); sqlite3_bind_int64(s,2,unix_now()); std::optional<Session> result; if(sqlite3_step(s)==SQLITE_ROW) result=Session{column_text(s,0),column_text(s,2),sqlite3_column_int64(s,1),column_text(s,3)}; sqlite3_finalize(s); return result; }
void Database::delete_expired_sessions() { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"DELETE FROM sessions WHERE CAST(expires_at AS INTEGER)<=?",-1,&s,nullptr),db_,"prepare session cleanup"); sqlite3_bind_int64(s,1,unix_now()); require_sqlite(sqlite3_step(s),db_,"session cleanup"); sqlite3_finalize(s); }
std::optional<Town> Database::get_town(std::int64_t account_id) const { std::lock_guard lock(mutex_); sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"SELECT account_id,payload,revision,updated_at FROM towns WHERE account_id=?",-1,&s,nullptr),db_,"prepare town lookup"); sqlite3_bind_int64(s,1,account_id); std::optional<Town> result; if(sqlite3_step(s)==SQLITE_ROW){const auto*blob=sqlite3_column_blob(s,1); result=Town{sqlite3_column_int64(s,0),std::string(static_cast<const char*>(blob),sqlite3_column_bytes(s,1)),sqlite3_column_int64(s,2),column_text(s,3)};} sqlite3_finalize(s); return result; }
bool Database::save_town(const Town& expected,const std::string& backup_payload) { std::lock_guard lock(mutex_); exec("BEGIN IMMEDIATE"); try { sqlite3_stmt*s=nullptr; require_sqlite(sqlite3_prepare_v2(db_,"INSERT INTO towns(account_id,payload,revision) VALUES(?,?,?) ON CONFLICT(account_id) DO UPDATE SET payload=excluded.payload,revision=excluded.revision,updated_at=CURRENT_TIMESTAMP WHERE towns.revision=?",-1,&s,nullptr),db_,"prepare town save"); sqlite3_bind_int64(s,1,expected.account_id); sqlite3_bind_blob(s,2,expected.payload.data(),static_cast<int>(expected.payload.size()),SQLITE_TRANSIENT); sqlite3_bind_int64(s,3,expected.revision+1); sqlite3_bind_int64(s,4,expected.revision); bool ok=sqlite3_step(s)==SQLITE_DONE && sqlite3_changes(db_)==1; sqlite3_finalize(s); if(!ok){exec("ROLLBACK");return false;} if(!backup_payload.empty()){require_sqlite(sqlite3_prepare_v2(db_,"INSERT INTO town_backups(account_id,revision,payload) VALUES(?,?,?)",-1,&s,nullptr),db_,"prepare town backup"); sqlite3_bind_int64(s,1,expected.account_id);sqlite3_bind_int64(s,2,expected.revision);sqlite3_bind_blob(s,3,backup_payload.data(),static_cast<int>(backup_payload.size()),SQLITE_TRANSIENT);require_sqlite(sqlite3_step(s),db_,"town backup");sqlite3_finalize(s);} exec("COMMIT"); return true; } catch(...) { exec("ROLLBACK"); throw; } }
}
