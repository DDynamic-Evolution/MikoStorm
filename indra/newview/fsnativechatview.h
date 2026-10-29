// UI-thread helpers for native tabs and a Rich Edit reading position.
#ifndef FS_NATIVE_CHAT_VIEW_H
#define FS_NATIVE_CHAT_VIEW_H
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>
namespace FSNativeChat
{
struct TabItem
{
    std::string key;
    std::wstring label;
    std::uint64_t token = 0;
};
class TabStrip
{
public:
    void clear(HWND control) { SendMessageW(control, TCM_DELETEALLITEMS, 0, 0); mItems.clear(); }
    bool contains(const std::string& key, std::uint64_t token) const
    { return std::any_of(mItems.begin(), mItems.end(), [&](const auto& x) { return x.key == key && x.token == token; }); }
    const TabItem* selected(HWND control) const
    {
        const auto index = SendMessageW(control, TCM_GETCURSEL, 0, 0);
        return index >= 0 && static_cast<std::size_t>(index) < mItems.size() ? &mItems[static_cast<std::size_t>(index)] : nullptr;
    }
    void select(HWND control, const std::string& key, std::uint64_t token) const
    {
        const auto it = std::find_if(mItems.begin(),mItems.end(),[&](const auto& x){return x.key==key && x.token==token;});
        const LRESULT index = it == mItems.end() ? -1 : static_cast<LRESULT>(it-mItems.begin());
        if (SendMessageW(control, TCM_GETCURSEL, 0, 0) != index)
            SendMessageW(control, TCM_SETCURSEL, static_cast<WPARAM>(index), 0);
    }
    bool update(HWND control, const std::vector<TabItem>& incoming, const std::string& activeKey, std::uint64_t activeToken)
    {
        // Label/unread changes must not recreate tabs or reset their overflow position.
        for (std::size_t n=mItems.size();n>0;--n)
        {
            const auto i=n-1;
            if (std::none_of(incoming.begin(),incoming.end(),[&](const auto& x){return x.key==mItems[i].key && x.token==mItems[i].token;}))
            { SendMessageW(control,TCM_DELETEITEM,i,0);mItems.erase(mItems.begin()+static_cast<std::ptrdiff_t>(i)); }
        }
        for (const auto& item : incoming)
        {
            auto it=std::find_if(mItems.begin(),mItems.end(),[&](const auto& x){return x.key==item.key && x.token==item.token;});
            TCITEMW native{};native.mask=TCIF_TEXT;native.pszText=const_cast<wchar_t*>(item.label.c_str());
            if (it==mItems.end())
            {
                if (SendMessageW(control,TCM_INSERTITEMW,mItems.size(),reinterpret_cast<LPARAM>(&native))<0) return false;
                mItems.push_back(item);
            }
            else if (it->label!=item.label)
            {
                if (!SendMessageW(control,TCM_SETITEMW,static_cast<WPARAM>(it-mItems.begin()),reinterpret_cast<LPARAM>(&native))) return false;
                it->label=item.label;
            }
        }
        select(control,activeKey,activeToken);return true;
    }
private:
    std::vector<TabItem> mItems;
};
class HistoryView
{
public:
    static std::wstring normalize(const std::wstring& text)
    {
        std::wstring result;result.reserve(text.size());
        for (std::size_t i=0;i<text.size();++i)
        {
            const wchar_t ch=text[i];result+=ch==L'\n'?L'\r':ch;
            if (ch==L'\r' && i+1<text.size() && text[i+1]==L'\n') ++i;
        }
        return result;
    }
    static bool atBottom(HWND control)
    {
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
        if (!GetScrollInfo(control,SB_VERT,&info)) return true;
        const auto last=(std::max)(static_cast<std::int64_t>(info.nMin),static_cast<std::int64_t>(info.nMax)-(info.nPage?static_cast<std::int64_t>(info.nPage)-1:0));
        return static_cast<std::int64_t>(info.nPos)>=last-1;
    }
    // Capture a reading position without treating focus/layout changes as user input.
    void remember(HWND control)
    {
        if (!mInitialized) return;
        SendMessageW(control,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&mSelection));
        const auto first=SendMessageW(control,EM_GETFIRSTVISIBLELINE,0,0);
        const auto index=SendMessageW(control,EM_LINEINDEX,static_cast<WPARAM>(first),0);
        mAnchor=index>=0?static_cast<LONG>(index):0;
    }
    void observeUser(HWND control)
    {
        // Programmatic scrolling during replacement/Latest can reenter the subclass.
        if (mUpdating || !mInitialized) return;
        remember(control);
        mFollowing=mEnabled && mSelection.cpMin==mSelection.cpMax && atBottom(control);
        if (mFollowing) mNewMessages=0;
    }
    void replace(HWND control,const std::wstring& text,std::size_t removedChars,
                 std::uint64_t count,std::uint64_t dropped,bool snapshot,bool gap=false)
    {
        if (!snapshot) remember(control);
        UpdateGuard update(mUpdating);
        if (!mInitialized) { mFollowing=mEnabled;mSelection={0,0};mAnchor=0; }
        else if (gap || (snapshot && dropped!=mDropped)) { mSelection={0,0};mAnchor=0; }
        else if (!snapshot && removedChars)
        {
            const LONG removed=static_cast<LONG>((std::min)(removedChars,static_cast<std::size_t>((std::numeric_limits<LONG>::max)())));
            if (mSelection.cpMin<removed) mSelection={0,0};
            else { mSelection.cpMin-=removed;mSelection.cpMax-=removed; }
            mAnchor=(std::max)(0L,mAnchor-removed);
        }
        const auto delta=mInitialized && count>=mCount?count-mCount:0;
        if (mEnabled && mFollowing) mNewMessages=0;
        else mNewMessages=static_cast<unsigned>((std::min)(std::uint64_t{999},static_cast<std::uint64_t>(mNewMessages)+delta));
        const auto display=normalize(text);
        SendMessageW(control,WM_SETREDRAW,FALSE,0);SetWindowTextW(control,display.c_str());
        mInitialized=true;mCount=count;mDropped=dropped;
        // Re-enable layout before positioning the viewport, including in inactive windows.
        SendMessageW(control,WM_SETREDRAW,TRUE,0);
        if (mEnabled && mFollowing) scrollToEnd(control);
        else
        {
            GETTEXTLENGTHEX length{};length.flags=GTL_PRECISE|GTL_NUMCHARS;length.codepage=1200;
            const auto chars=SendMessageW(control,EM_GETTEXTLENGTHEX,reinterpret_cast<WPARAM>(&length),0);
            const LONG size=static_cast<LONG>((std::max)(LRESULT{0},(std::min)(chars,static_cast<LRESULT>((std::numeric_limits<LONG>::max)()))));
            mSelection.cpMin=(std::min)(mSelection.cpMin,size);mSelection.cpMax=(std::min)(mSelection.cpMax,size);
            SendMessageW(control,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&mSelection));
            const auto target=SendMessageW(control,EM_LINEFROMCHAR,static_cast<WPARAM>((std::min)(mAnchor,size)),0);
            const auto first=SendMessageW(control,EM_GETFIRSTVISIBLELINE,0,0);
            SendMessageW(control,EM_LINESCROLL,0,target-first);
        }
        RedrawWindow(control,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);
    }
    void jumpToLatest(HWND control)
    {
        UpdateGuard update(mUpdating);
        mEnabled=true;mFollowing=true;mNewMessages=0;scrollToEnd(control);remember(control);
    }
    void enable(HWND control,bool value)
    { if (value) jumpToLatest(control); else { remember(control);mEnabled=false;mFollowing=false; } }
    bool enabled() const { return mEnabled; }
    unsigned newMessages() const { return mNewMessages; }
    std::uint64_t dropped() const { return mDropped; }
private:
    struct UpdateGuard
    {
        explicit UpdateGuard(bool& flag) : active(flag), previous(flag) { active=true; }
        ~UpdateGuard() { active=previous; }
        bool& active;
        bool previous;
    };
    static void scrollToEnd(HWND control)
    {
        SendMessageW(control,EM_SETSEL,static_cast<WPARAM>(-1),-1);
        // Scroll the viewport directly; no caret visibility, activation or focus required.
        SendMessageW(control,WM_VSCROLL,SB_BOTTOM,0);
    }
    bool mEnabled=true,mFollowing=true,mInitialized=false,mUpdating=false;
    LONG mAnchor=0;
    CHARRANGE mSelection{0,0};
    std::uint64_t mCount=0,mDropped=0;
    unsigned mNewMessages=0;
};
}
#endif
