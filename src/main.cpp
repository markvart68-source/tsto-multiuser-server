#include "config/config.hpp"
#include "database/database.hpp"
#include "auth/auth_service.hpp"
#include "services/town_service.hpp"
#include "http/api_router.hpp"
#include <iostream>
int main(){ try { auto config=tsto::load_config("config/server.json"); tsto::Database db(config.database_path); db.migrate(); tsto::AuthService auth(db,config); tsto::TownService towns(db); tsto::ApiRouter api(auth,towns); (void)api; std::cout<<"TSTO multi-user server initialized on "<<config.listen_address<<":"<<config.listen_port<<"\n"; std::cout<<"HTTP transport adapter is the next integration layer.\n"; } catch(const std::exception& e){ std::cerr<<"startup failed: "<<e.what()<<"\n"; return 1;} }
