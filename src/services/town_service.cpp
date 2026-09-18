#include "services/town_service.hpp"
#include <stdexcept>
namespace tsto {
Town TownService::load(std::int64_t id){ auto t=db_.get_town(id); if(!t) throw std::runtime_error("town not found"); return *t; }
Town TownService::save(std::int64_t id,std::int64_t rev,std::string payload){ Town t{id,std::move(payload),rev,{}}; if(!db_.save_town(t,"")) throw std::runtime_error("town revision conflict"); auto saved=db_.get_town(id); if(!saved) throw std::runtime_error("town save failed"); return *saved; }
}
