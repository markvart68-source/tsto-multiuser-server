#include "auth/auth_service.hpp"
#include "database/database.hpp"
#include "services/town_service.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tsto;
int main(){
    const auto path=std::filesystem::temp_directory_path()/"tsto_multiuser_tests.db"; std::filesystem::remove(path);
    Database db(path); db.migrate(); Config config; config.session_ttl_seconds=3600; AuthService auth(db,config);
    auto alice=auth.register_account("alice@example.test","password-one","Alice","phone-a");
    assert(alice.account.id>0&&!alice.access_token.empty());
    auto aliceTablet=auth.login("alice@example.test","password-one","tablet-a");
    assert(aliceTablet.account.id==alice.account.id&&aliceTablet.access_token!=alice.access_token);
    auto bob=auth.register_account("bob@example.test","password-two","Bob","phone-b");
    assert(bob.account.id!=alice.account.id);
    assert(auth.authenticate("Bearer "+alice.access_token).account_id==alice.account.id);
    TownService towns(db); auto first=towns.load(alice.account.id); assert(first.revision==0);
    auto saved=towns.save(alice.account.id,0,"alice-town"); assert(saved.revision==1&&saved.payload=="alice-town");
    bool conflict=false; try{towns.save(alice.account.id,0,"stale");}catch(const TownConflict&){conflict=true;} assert(conflict);
    assert(towns.load(bob.account.id).payload.empty());
    bool invalid=false; try{auth.authenticate("Bearer invalid");}catch(const std::exception&){invalid=true;} assert(invalid);
    std::filesystem::remove(path); std::cout<<"all multi-user tests passed\n";
}
