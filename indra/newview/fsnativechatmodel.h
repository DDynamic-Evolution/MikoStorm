// Small, platform-independent transcript used by the native window and its bridge.
#ifndef FS_NATIVE_CHAT_MODEL_H
#define FS_NATIVE_CHAT_MODEL_H
#include <cstddef>
#include <deque>
#include <map>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
namespace FSNativeChat
{
enum class ThemeMode { System, Light, Dark, HighContrast };
inline ThemeMode chooseTheme(bool highContrast, bool knownAppMode, bool appsUseLight)
{
    if (highContrast) return ThemeMode::HighContrast;
    if (!knownAppMode) return ThemeMode::System;
    return appsUseLight ? ThemeMode::Light : ThemeMode::Dark;
}
struct FriendEntry
{
    std::string id, name;
    bool online = false;
    std::uint64_t token = 0;
    bool operator==(const FriendEntry&) const = default;
};
// Copied friend data only; no relationship pointers cross the thread boundary.
class Friends
{
public:
    static constexpr std::size_t MaxEntries = 4096;
    void update(std::vector<FriendEntry> entries)
    {
        if (entries.size() > MaxEntries) entries.resize(MaxEntries);
        std::map<std::string, std::uint64_t> next;
        for (auto& entry : entries)
        {
            const auto old = mTokens.find(entry.id);
            entry.token = old == mTokens.end() ? ++mNextToken : old->second;
            next.emplace(entry.id, entry.token);
        }
        mTokens = std::move(next); mEntries = std::move(entries);
    }
    bool matches(const std::string& id, std::uint64_t token) const
    { const auto it = mTokens.find(id); return it != mTokens.end() && it->second == token; }
    void clear() { mTokens.clear(); mEntries.clear(); }
    const std::vector<FriendEntry>& entries() const { return mEntries; }
private:
    std::uint64_t mNextToken = 0;
    std::map<std::string, std::uint64_t> mTokens;
    std::vector<FriendEntry> mEntries;
};
class Transcript
{
public:
    static constexpr std::size_t MaxEntries = 200, MaxBytes = 128 * 1024, MaxEntryBytes = 8192;
    void append(std::string line)
    {
        if (line.size() > MaxEntryBytes)
        {
            auto end = MaxEntryBytes - 3;
            while (end && (static_cast<unsigned char>(line[end]) & 0xc0) == 0x80) --end;
            line.resize(end); line += "...";
        }
        mBytes += line.size(); mLines.push_back(std::move(line));
        while (mLines.size() > MaxEntries || mBytes > MaxBytes)
        { mBytes -= mLines.front().size(); mLines.pop_front(); ++mDropped; }
    }
    void clear() { mLines.clear(); mBytes = 0; mDropped = 0; }
    const std::deque<std::string>& lines() const { return mLines; }
    std::size_t dropped() const { return mDropped; }
    std::size_t bytes() const { return mBytes; }
private:
    std::deque<std::string> mLines;
    std::size_t mBytes = 0, mDropped = 0;
};
// Main-thread model. Keys identify conversations; tokens identify their lifetime.
// A removed/recreated conversation must never accept an old queued submission.
class Conversations
{
public:
    enum class Kind { Local, IM, Group };
    struct Conversation
    {
        std::string key, label, draft;
        Kind kind = Kind::Local;
        std::uint64_t token = 0;
        Transcript history;
        unsigned unread = 0;
    };
    static constexpr std::size_t MaxConversations = 64;
    Conversations() { clear(); }
    void clear()
    {
        mItems.clear(); mSelected.clear();
        Conversation local; local.token = ++mNextToken;
        mItems.emplace("", std::move(local));
    }
    Conversation* find(const std::string& key)
    { const auto it = mItems.find(key); return it == mItems.end() ? nullptr : &it->second; }
    bool matches(const std::string& key, std::uint64_t token)
    { const auto* item = find(key); return item && item->token == token; }
    Conversation* upsert(const std::string& key, const std::string& label, Kind kind)
    {
        auto* item = find(key);
        if (!item)
        {
            if (mItems.size() >= MaxConversations) return nullptr;
            Conversation fresh; fresh.key = key; fresh.token = ++mNextToken;
            item = &mItems.emplace(key, std::move(fresh)).first->second;
        }
        item->label = label; item->kind = kind; return item;
    }
    bool select(const std::string& key, std::uint64_t token)
    {
        if (!matches(key, token)) return false;
        mSelected = key; selected().unread = 0; return true;
    }
    void erase(const std::string& key)
    {
        if (key.empty()) return;
        mItems.erase(key); if (mSelected == key) mSelected.clear();
    }
    void rekey(const std::string& oldKey, const std::string& newKey)
    {
        if (oldKey.empty() || newKey.empty() || oldKey == newKey) return;
        auto it = mItems.find(oldKey); if (it == mItems.end()) return;
        // Never overwrite another live conversation's draft on an ID collision.
        if (mItems.find(newKey) != mItems.end()) { erase(oldKey); return; }
        auto moved = std::move(it->second); mItems.erase(it);
        moved.key = newKey; moved.token = ++mNextToken;
        mItems.insert_or_assign(newKey, std::move(moved));
        if (mSelected == oldKey) mSelected = newKey;
    }
    Conversation& selected() { return mItems.at(mSelected); }
    Conversation& local() { return mItems.at(""); }
    const std::map<std::string, Conversation>& items() const { return mItems; }
private:
    std::map<std::string, Conversation> mItems;
    std::string mSelected;
    std::uint64_t mNextToken = 0;
};
}
#endif
