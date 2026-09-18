#pragma once
#include "database/database.hpp"
#include <stdexcept>
#include <string>
namespace tsto {
class TownConflict final : public std::runtime_error { public: TownConflict():std::runtime_error("town revision conflict"){} };
class TownService {
public:
    explicit TownService(Database& db): db_(db) {}
    Town load(std::int64_t account_id);
    Town save(std::int64_t account_id, std::int64_t expected_revision, std::string payload);
private: Database& db_;
};
}
