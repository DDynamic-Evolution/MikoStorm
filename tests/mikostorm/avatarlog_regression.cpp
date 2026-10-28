#include "viewer_test_stubs.h"
#include "fsmikoavatarlog.h"
#include <chrono>
#include <stdexcept>
#include <thread>
std::size_t lines(const std::filesystem::path& path)
{
    std::ifstream file(path); std::string line; std::size_t count=0;
    while (std::getline(file,line)) ++count;
    return count;
}
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try
    {
        auto folder=std::filesystem::current_path() / ("avatar-log-regression-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(folder);
        auto first=folder/"first.txt", second=folder/"second.txt", bad=folder/"missing"/"retry.txt", persisted=folder/"persisted.txt";
        auto& log=FSMikoAvatarLog::instance();
        const LLUUID one("10000000-0000-0000-0000-000000000001"), two("10000000-0000-0000-0000-000000000002"), three("10000000-0000-0000-0000-000000000003"), four("10000000-0000-0000-0000-000000000004");
        gSavedSettings.path=first.string();
        log.logAvatar(one); log.logAvatar(one);
        require(lines(first)==1,"synchronous names must deduplicate successful writes");
        LLAvatarNameCache::deferred=true;
        log.logAvatar(two);
        gSavedSettings.path=second.string();
        LLAvatarNameCache::deferred=false;
        log.logAvatar(one);
        LLAvatarNameCache::resolve(two);
        require(lines(first)==1 && lines(second)==1,"path switch must cancel old callbacks and reload seen IDs");
        LLAvatarNameCache::deferred=true;
        log.logAvatar(three); log.logAvatar(three);
        require(LLAvatarNameCache::pending[three]->num_slots()==1,"pending lookups must deduplicate");
        gSavedSettings.enabled=false;
        LLAvatarNameCache::resolve(three);
        require(lines(second)==1,"disabled log must reject late callback");
        gSavedSettings.enabled=true;
        LLAvatarNameCache::deferred=false;
        log.logAvatar(three);
        require(lines(second)==2,"cancelled callback must remain retryable");
        gSavedSettings.path=bad.string();
        log.logAvatar(four);
        require(!std::filesystem::exists(bad),"invalid path must not create an entry");
        std::filesystem::create_directory(bad.parent_path());
        log.logAvatar(four);
        require(!std::filesystem::exists(bad),"failed writes must back off");
        std::this_thread::sleep_for(std::chrono::milliseconds(10100));
        log.logAvatar(four); log.logAvatar(four);
        require(lines(bad)==1,"failed write must retry and deduplicate after recovery");
        { std::ofstream file(persisted); file << "time | name | user | " << one.asString() << '\n'; }
        gSavedSettings.path=persisted.string();
        log.logAvatar(one); log.logAvatar(gAgentID);
        require(lines(persisted)==1,"persistent IDs and self must be skipped");
        for (const auto& path : {first,second,bad,persisted}) std::filesystem::remove(path);
        std::filesystem::remove(bad.parent_path());
        std::filesystem::remove(folder);
        std::cout << "Avatar log: 9 behavioral checks passed\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
