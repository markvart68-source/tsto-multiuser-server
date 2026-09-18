#include "services/town_service.hpp"
#include <stdexcept>
namespace tsto {
Town TownService::load(std::int64_t account_id) {
    auto town = db_.get_town(account_id);
    if (!town) {
        return Town{account_id, {}, 0, {}};
    }
    return *town;
}
Town TownService::save(std::int64_t account_id, std::int64_t expected_revision, std::string payload) {
    auto current = db_.get_town(account_id);
    if (current && current->revision != expected_revision) throw TownConflict();
    Town expected{account_id, std::move(payload), expected_revision, {}};
    const std::string previous = current ? current->payload : std::string{};
    if (!db_.save_town(expected, previous)) throw TownConflict();
    return load(account_id);
}
}
