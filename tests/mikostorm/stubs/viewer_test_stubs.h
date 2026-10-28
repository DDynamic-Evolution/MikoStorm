#pragma once
#include <boost/signals2.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#define LL_WINDOWS 1
#endif
#define LL_WARNS(tag) std::clog
#define LL_INFOS(tag) std::clog
#define LL_ENDL std::endl
class LLUUID
{
    std::string mText;
public:
    LLUUID() = default;
    explicit LLUUID(const std::string& text):mText(text){}
    bool isNull() const { return mText.empty(); }
    std::string asString() const { return mText; }
    auto operator<=>(const LLUUID&) const = default;
    friend std::ostream& operator<<(std::ostream& out,const LLUUID& id) { return out << id.mText; }
};
inline LLUUID gAgentID("00000000-0000-0000-0000-000000000001");
template<class T> class LLSingleton
{
public:
    static T& instance() { static T value; return value; }
};
#define LLSINGLETON(T) friend class LLSingleton<T>; T()
class TestSettings
{
public:
    bool enabled{true};
    std::string path;
    bool getBOOL(const std::string&) const { return enabled; }
    std::string getString(const std::string&) const { return path; }
};
inline TestSettings gSavedSettings;
template<class T> class LLCachedControl
{
    TestSettings& mSettings;
public:
    LLCachedControl(TestSettings& settings,const std::string&,T):mSettings(settings){}
    T operator()() const { return mSettings.enabled; }
};
inline constexpr int LL_PATH_USER_SETTINGS=0;
struct TestDir
{
    std::string getExpandedFilename(int,const std::string& file) { return (std::filesystem::temp_directory_path()/file).string(); }
};
inline TestDir testDir;
inline TestDir* gDirUtilp=&testDir;
using llifstream=std::ifstream;
using llofstream=std::ofstream;
struct LLStringUtil
{
    static void trim(std::string& text)
    {
        const auto start=text.find_first_not_of(" \t\r\n");
        text=start==text.npos ? "" : text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);
    }
};
class LLAvatarName
{
public:
    std::string getDisplayName(bool) const { return "Display Name"; }
    std::string getUserName() const { return "test.user"; }
};
struct LLAvatarNameCache
{
    using Signal=boost::signals2::signal<void(const LLUUID&,const LLAvatarName&)>;
    inline static bool deferred=false;
    inline static std::map<LLUUID,std::shared_ptr<Signal>> pending;
    static boost::signals2::connection get(const LLUUID& id,std::function<void(const LLUUID&,const LLAvatarName&)> callback)
    {
        if (!deferred) { callback(id,LLAvatarName{}); return {}; }
        auto& signal=pending[id];
        if (!signal) signal=std::make_shared<Signal>();
        return signal->connect(callback);
    }
    static void resolve(const LLUUID& id)
    {
        auto found=pending.find(id);
        if (found==pending.end()) return;
        auto signal=found->second;
        pending.erase(found);
        (*signal)(id,LLAvatarName{});
    }
};
class LLCharacter
{
public:
    virtual ~LLCharacter()=default;
    inline static std::vector<LLCharacter*> sInstances;
};
class LLVOAvatar:public LLCharacter
{
    LLUUID mId;
public:
    explicit LLVOAvatar(LLUUID id):mId(id){}
    const LLUUID& getID() const { return mId; }
};
