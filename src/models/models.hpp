#pragma once
#include <cstdint>
#include <string>

namespace tsto {
struct Account { std::int64_t id{}; std::string email, password_hash, display_name, user_id; std::int64_t mayhem_id{}; };
struct Device { std::string id, platform, app_version; std::int64_t account_id{}; };
struct Session { std::string token_hash, device_id; std::int64_t account_id{}; std::string expires_at; };
struct Town { std::int64_t account_id{}; std::string payload; std::int64_t revision{}; std::string updated_at; };
}
