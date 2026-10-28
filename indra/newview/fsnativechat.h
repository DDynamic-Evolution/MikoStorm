// Native local/IM/group chat window. All entry points below are viewer-main-thread only.
#ifndef FS_NATIVE_CHAT_H
#define FS_NATIVE_CHAT_H
#include "llpreprocessor.h"
#include <string>
class LLChat;
class LLUUID;
class LLSD;
namespace FSNativeChat
{
#if LL_WINDOWS
void initialize();
void shutdown();
void show();
bool isOpen();
void append(const LLChat& chat, bool archive, const LLSD& args);
void appendIM(const LLUUID& session, const std::string& name, const LLUUID& sender,
              const std::string& text, const std::string& time);
void removeIM(const LLUUID& session);
void rekeyIM(const LLUUID& oldSession, const LLUUID& newSession);
inline bool supported() { return true; }
#else
inline void initialize() {}
inline void shutdown() {}
inline void show() {}
inline bool isOpen() { return false; }
inline void append(const LLChat&, bool, const LLSD&) {}
inline void appendIM(const LLUUID&, const std::string&, const LLUUID&, const std::string&, const std::string&) {}
inline void removeIM(const LLUUID&) {}
inline void rekeyIM(const LLUUID&, const LLUUID&) {}
inline bool supported() { return false; }
#endif
}
#endif
