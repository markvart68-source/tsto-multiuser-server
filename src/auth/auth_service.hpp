#include "auth/auth_service.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace tsto {
namespace {
std::string hex_encode(const unsigned char* bytes, std::size_t size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return out.str();
}

std::string random_hex(std::size_t length) {
    std::vector<unsigned char> bytes(length);
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("secure random generation failed");
    }
    return hex_encode(bytes.data(), bytes.size());
}

std::string hash_token(const std::string& token) {
    unsigned char digest[SHA256_DIGEST_LENGTH] = {};
    SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), digest);
    return hex_encode(digest, SHA256_DIGEST_LENGTH);
}

std::string hash_password(const std::string& password) {
    const std::string salt = random_hex(16);
    unsigned char output[32] = {};
    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
        120000, EVP_sha256(), sizeof(output), output);
    return "pbkdf2$120000$" + salt + "$" + hex_encode(output, sizeof(output));
}

bool verify_password(const std::string& password, const std::string& encoded) {
    const std::size_t part1 = encoded.find('$');
    const std::size_t part2 = encoded.find('$', part1 + 1);
    const std::size_t part3 = encoded.find('$', part2 + 1);
    if (part1 == std::string::npos || part2 == std::string::npos || part3 == std::string::npos) {
        return false;
    }

    const std::string version = encoded.substr(0, part1);
    if (version != "pbkdf2") {
        return false;
    }

    const std::string iterations_text = encoded.substr(part1 + 1, part2 - part1 - 1);
    const std::string salt = encoded.substr(part2 + 1, part3 - part2 - 1);
    const std::string expected = encoded.substr(part3 + 1);

    int iterations = 0;
    try {
        iterations = std::stoi(iterations_text);
    } catch (...) {
        return false;
    }

    unsigned char output[32] = {};
    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
        iterations, EVP_sha256(), sizeof(output), output);

    return hex_encode(output, sizeof(output)) == expected;
}

std::int64_t unix_now() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
}

AuthService::AuthService(Database& db, const Config& config) : db_(db), config_(config) {}

AuthResult AuthService::register_account(std::string email, std::string password, std::string display_name, std::string device_id) {
    if (email.empty() || password.size() < 8 || device_id.empty()) {
        throw std::invalid_argument("email, password, and device_id are required");
    }
    if (db_.find_account_by_email(email)) {
        throw std::runtime_error("account already exists");
    }

    Account input{};
    input.email = std::move(email);
    input.password_hash = hash_password(password);
    input.display_name = std::move(display_name);
    input.user_id = random_hex(12);
    input.mayhem_id = unix_now();

    auto account = db_.create_account(input);
    if (!account) {
        throw std::runtime_error("account creation failed");
    }
    if (!db_.add_device(Device{device_id, "unknown", "", account->id})) {
        throw std::runtime_error("device registration failed");
    }
    return login(account->email, password, device_id);
}

AuthResult AuthService::login(std::string email, std::string password, std::string device_id) {
    if (email.empty() || password.empty() || device_id.empty()) {
        throw std::invalid_argument("email, password, and device_id are required");
    }

    auto account = db_.find_account_by_email(email);
    if (!account || !verify_password(password, account->password_hash)) {
        throw std::runtime_error("invalid credentials");
    }

    if (!db_.device_belongs_to(account->id, device_id)) {
        if (!db_.add_device(Device{device_id, "unknown", "", account->id})) {
            throw std::runtime_error("device registration failed");
        }
    }

    const std::string access_token = random_hex(32);
    const std::string expires_at = std::to_string(unix_now() + config_.session_ttl_seconds);
    Session session{hash_token(access_token), device_id, account->id, expires_at};
    if (!db_.create_session(session)) {
        throw std::runtime_error("session creation failed");
    }

    AuthResult result{*account, session, access_token};
    return result;
}

void AuthService::register_device(const Session& session, std::string device_id, std::string platform, std::string app_version) {
    if (!db_.add_device(Device{std::move(device_id), std::move(platform), std::move(app_version), session.account_id})) {
        throw std::runtime_error("device registration failed");
    }
}

Session AuthService::authenticate(const std::string& authorization_header) const {
    if (authorization_header.rfind("Bearer ", 0) != 0) {
        throw std::runtime_error("Bearer authorization header required");
    }
    const std::string token = authorization_header.substr(7);
    const auto session = db_.find_session(hash_token(token));
    if (!session) {
        throw std::runtime_error("invalid or expired session");
    }
    if (!db_.device_belongs_to(session->account_id, session->device_id)) {
        throw std::runtime_error("session device not authorized");
    }
    return *session;
}

Account AuthService::account(std::int64_t account_id) const {
    const auto account = db_.find_account_by_id(account_id);
    if (!account) {
        throw std::runtime_error("account not found");
    }
    return *account;
}

std::map<std::string, std::int64_t> AuthService::currencies(std::int64_t account_id) const {
    return db_.get_currencies(account_id);
}

void AuthService::set_currency(std::int64_t account_id, const std::string& currency, std::int64_t amount) {
    if (currency.empty() || amount < 0) {
        throw std::invalid_argument("currency and amount are required");
    }
    if (!db_.set_currency(account_id, currency, amount)) {
        throw std::runtime_error("unable to update currency balance");
    }
}
}
