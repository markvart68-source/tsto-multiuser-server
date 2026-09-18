#include "config/config.hpp"
#include "database/database.hpp"
#include "auth/auth_service.hpp"
#include "services/town_service.hpp"
#include "http/api_router.hpp"
#include "http/http_server.hpp"
#include <iostream>

int main() {
    try {
        const auto config = tsto::load_config("config/server.json");
        tsto::Database db(config.database_path);
        db.migrate();
        tsto::AuthService auth(db, config);
        tsto::TownService towns(db);
        tsto::ApiRouter router(auth, towns);
        tsto::HttpServer server(config, router, "webpanel");

        std::cout << "TSTO server listening at http://" << config.listen_address << ':' << config.listen_port << "/webpanel/\n";
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "startup failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
