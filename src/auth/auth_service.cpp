#include "auth/auth_service.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tsto { namespace {
std::string hex(const unsigned char* data, std::size_t length) {
    std::ostringstream out;
    for (std::size_t i = 0; i < length; ++i) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]);
    return out.str();
}
std::string random_hex(std::size_t bytes) {
    std::vector<unsigned char> value(bytes);
    if (RAND_bytes(value.data(), static_cast<int>(value.size())) != 1) throw std::runtime_error("secure random generation failed");
    return hex(value.data(), value.size());
}
std::string sha256(const std::string& value) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest);
    return hex(digest, sizeof digest);
}
std::string password_hash(const std::string& password) {
    constexpr int rounds = 120000;
    const auto salt = random_hex(16);
    unsigned char digest[32];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()), rounds, EVP_sha256(), sizeof digest, digest) != 1) throw std::runtime_error("password hashing failed");
    return "pbkdf2$" + std::to_string(rounds) + "$" + salt + "$" + hex(digest, sizeof digest);
}
bool password_matches(const std::string& password, const std::string& stored) {
    const auto p1 = stored.find('$'), p2 = stored.find('$', p1 == std::string::npos ? 0 : p1 + 1), p3 = stored.find('$', p2 == std::string::npos ? 0 : p2 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos || stored.substr(0, p1) != "pbkdf2") return false;
    int rounds = 0;
    try { rounds = std::stoi(stored.substr(p1 + 1, p2 - p1 - 1)); } catch (...) { return false; }
    if (rounds < 10000 || rounds > 10000000) return false;
    const auto salt = stored.substr(p2 + 1, p3 - p2 - 1);
    unsigned char digest[32];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()), rounds, EVP_sha256(), sizeof digest, digest) != 1) return false;
    return hex(digest, sizeof digest) == stored.substr(p3 + 1);
}
std::int64_t epoch_now() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
}
AuthService::AuthService(Database& db, const Config& config) : db_(db), config_(config) {}
AuthResult AuthService::register_account(std::string email, std::string password, std::string display_name, std::string device_id) {
    if (email.empty() || password.size() < 8 || device_id.empty()) throw std::invalid_argument("email, device_id, and an 8-character password are required");
    if (db_.find_account_by_email(email)) throw std::runtime_error("account already exists");
    Account draft{0, std::move(email), password_hash(password), std::move(display_name), random_hex(16), static_cast<std::int64_t>(epoch_now()) ^ static_cast<std::int64_t>(RAND_MAX)};
    auto account = db_.create_account(draft);
    if (!account) throw std::runtime_error("account creation failed");
    if (!db_.add_device(Device{device_id, "unknown", "", account->id})) throw std::runtime_error("device registration failed");
    return login(account->email, password, device_id);
}
AuthResult AuthService::login(std::string email, std::string password, std::string device_id) {
    auto account = db_.find_account_by_email(email);
    if (!account || !password_matches(password, account->password_hash)) throw std::runtime_error("invalid credentials");
    if (!db_.device_belongs_to(account->id, device_id) && !db_.add_device(Device{device_id, "unknown", "", account->id})) throw std::runtime_error("device registration failed");
    const auto token = random_hex(32);
    Session session{sha256(token), device_id, account->id, std::to_string(epoch_now() + config_.session_ttl_seconds)};
    if (!db_.create_session(session)) throw std::runtime_error("session creation failed");
    return {*account, session, token};
}
void AuthService::register_device(const Session& session, std::string device_id, std::string platform, std::string app_version) {
    if (device_id.empty()) throw std::invalid_argument("device_id is required");
    if (!db_.add_device(Device{std::move(device_id), std::move(platform), std::move(app_version), session.account_id})) throw std::runtime_error("device registration failed");
}
Session AuthService::authenticate(const std::string& authorization) const {
    if (authorization.rfind("Bearer ", 0) != 0 || authorization.size() <= 7) throw std::runtime_error("bearer token required");
    auto session = db_.find_session(sha256(authorization.substr(7)));
    if (!session || !db_.device_belongs_to(session->account_id, session->device_id)) throw std::runtime_error("invalid or expired session");
    return *session;
}
}
