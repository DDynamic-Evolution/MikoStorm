"""Static native-chat integration checks; never start or compile the viewer."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
VIEWER = ROOT/'indra/newview'
def source(name): return (VIEWER/name).read_text(encoding='utf-8-sig')

class NativeChatSource(unittest.TestCase):
    def test_menu_is_peer_and_registered(self):
        for lang in ['en', 'de']:
            menu = ET.fromstring(source(f'skins/default/xui/{lang}/menu_viewer.xml'))
            communicate = menu.find(".//menu[@name='Communicate']")
            self.assertIsNotNone(communicate.find("menu_item_call[@name='Native Nearby Chat']"))
            self.assertFalse(communicate.findall("menu_item_check/menu_item_call"))
        cpp = source('llviewermenu.cpp')
        self.assertIn('commit.add("NativeChat.Show"', cpp)
        self.assertIn('enable.add("NativeChat.Supported"', cpp)

    def test_toolbar_and_translations(self):
        en = ET.fromstring(source('skins/default/xui/en/floater_fs_nearby_chat.xml'))
        de = ET.fromstring(source('skins/default/xui/de/floater_fs_nearby_chat.xml'))
        button = en.find(".//layout_panel[@name='lp_native_chat_btn']/button[@name='native_chat_btn']")
        self.assertEqual(button.find('button.commit_callback').get('function'), 'NativeChat.Show')
        self.assertIsNotNone(de.find(".//button[@name='native_chat_btn']"))
        names = set(re.findall(r'(?:translated|LLTrans::getString)\("(NativeChat[^\"]+)"\)', source('fsnativechatwin32.cpp')))
        for lang in ['en', 'de']:
            strings = ET.fromstring(source(f'skins/default/xui/{lang}/strings.xml'))
            declared = {e.get('name') for e in strings.findall('string')}
            self.assertTrue(names <= declared, names - declared)
            notifications = ET.fromstring(source(f'skins/default/xui/{lang}/notifications.xml'))
            self.assertIsNotNone(notifications.find("notification[@name='NativeChatOpenFailed']"))

    def test_thread_boundary_and_window_lifetime(self):
        cpp = source('fsnativechatwin32.cpp')
        ui = cpp[cpp.index('class NativeWindow'):cpp.index('// VIEWER MAIN THREAD ONLY.')]
        for forbidden in ['gSavedSettings.', 'gAgent.', 'FSNearbyChat::', 'LLWeb::', 'LLTrans::']:
            self.assertNotIn(forbidden, ui)
        self.assertIn('GetMessageW(', ui)
        self.assertIn('WS_EX_APPWINDOW', ui)
        self.assertIn('TM_PLAINTEXT', ui)
        self.assertIn('std::thread(', cpp)
        cleanup = source('llappviewer.cpp').split('bool LLAppViewer::cleanup()', 1)[1]
        self.assertLess(cleanup.index('FSNativeChat::shutdown()'), cleanup.index('LLNotifications::instance().clear()'))
        self.assertIn('mThread.join()', cpp)

    def test_chat_path_and_session_validation(self):
        floater = source('fsfloaternearbychat.cpp')
        fn = floater.split('void FSFloaterNearbyChat::addMessage(', 1)[1].split('void FSFloaterNearbyChat::', 1)[0]
        self.assertLess(fn.index('FSNativeChat::append('), fn.index('if (fsHideLocalChat)'))
        self.assertLess(fn.index('if (!chat.mMuted)'), fn.index('FSNativeChat::append('))
        cpp = source('fsnativechatwin32.cpp')
        self.assertIn('args["is_replay"].asBoolean()', cpp)
        self.assertIn('chat.mChatType == CHAT_TYPE_IM_GROUP', cpp)
        self.assertIn('RlvUtil::filterNames(text)', cpp)
        self.assertIn('command.generation != mGeneration', cpp)
        self.assertIn('mGeneration == mBridge->generation && mConversation == mBridge->conversation && mToken == mBridge->token', cpp)
        self.assertIn('FSNearbyChat::instance().sendChat(text,', cpp)
        self.assertNotIn('really_send_chat_from_viewer', cpp)
        self.assertNotIn('gMessageSystem', cpp)

    def test_hide_chat_only_hides_viewer_presentation(self):
        fn = source('fsfloaternearbychat.cpp').split('void FSFloaterNearbyChat::addMessage(', 1)[1].split('void FSFloaterNearbyChat::', 1)[0]
        self.assertEqual(fn.count('FSNativeChat::append('), 1)
        self.assertLess(fn.index('tmp_chat.mTimeStr = appendTime()'), fn.index('FSNativeChat::append('))
        self.assertLess(fn.index('FSNativeChat::append('), fn.index('if (fsHideLocalChat)'))
        self.assertLess(fn.index('if (fsHideLocalChat)'), fn.index('mChatHistoryMuted->appendMessage('))
        manager = source('fsnativechatwin32.cpp').split('void append(const LLChat&', 1)[1].split('void appendIM(', 1)[0]
        self.assertNotIn('FSHideLocalChat', manager)
        for guard in ['!archive', 'args["is_replay"].asBoolean()', 'chat.mMuted', 'CHAT_TYPE_IM_GROUP', 'filter(name, text']:
            self.assertIn(guard, manager)
        handler = source('llfloaterimnearbychathandler.cpp').split('void LLFloaterIMNearbyChatHandler::processChat(', 1)[1]
        accepted = handler.rindex('nearby_chat->addMessage(chat_msg, true, args)')
        self.assertLess(handler.index('CHAT_TYPE_DIRECT'), accepted)
        self.assertLess(handler.index('ShowScriptErrors'), accepted)
        self.assertLess(handler.index('CHAT_TYPE_IM && chat_msg.mSourceType == CHAT_SOURCE_OBJECT'), accepted)
        self.assertLess(accepted, handler.index('getBOOL("FSHideLocalChat")'))

    def test_background_following_uses_user_intent_without_focus(self):
        view = source('fsnativechatview.h')
        remember = view.split('void remember(HWND control)', 1)[1].split('void observeUser(', 1)[0]
        self.assertNotIn('mFollowing=', remember)
        for forbidden in ['SetFocus(', 'SetForegroundWindow(', 'GetForegroundWindow(', 'EM_SCROLLCARET']:
            self.assertNotIn(forbidden, view)
        self.assertIn('if (mUpdating || !mInitialized) return;', view)
        replace = view.split('void replace(HWND control', 1)[1].split('void jumpToLatest(', 1)[0]
        self.assertLess(replace.index('WM_SETREDRAW,TRUE'), replace.index('scrollToEnd(control)'))
        ui = source('fsnativechatwin32.cpp').split('static LRESULT CALLBACK historyProc(', 1)[1].split('static LRESULT CALLBACK inputProc(', 1)[0]
        self.assertIn('currentView().observeUser(hwnd)', ui)
        for message in ['WM_MOUSEWHEEL', 'WM_VSCROLL', 'WM_MOUSEMOVE', 'WM_KEYDOWN']:
            self.assertIn(message, ui)
        self.assertNotIn('WM_KILLFOCUS', ui)

    def test_accepted_im_path_and_no_history_import(self):
        cpp = source('llimview.cpp')
        accepted = cpp.split('bool LLIMModel::addToHistory(', 1)[1].split('bool LLIMModel::logToFile(', 1)[0]
        self.assertLess(accepted.index('session->addMessage('), accepted.index('FSNativeChat::appendIM('))
        self.assertIn('if (!is_announcement)', accepted)
        self.assertEqual(cpp.count('FSNativeChat::appendIM('), 1)
        native = source('fsnativechatwin32.cpp')
        self.assertNotIn('getMessagesSilently(', native)
        self.assertNotIn('loadHistory(', native)
        self.assertIn('LLMuteList::instance().isMuted', native)
        self.assertIn('RlvActions::canReceiveIM', native)

    def test_im_send_uses_floater_rules_without_editor_mutation(self):
        native = source('fsnativechatwin32.cpp')
        self.assertIn('floater->sendNativeMessage(', native)
        self.assertNotIn('LLIMModel::sendMessage(', native)
        send = source('fsfloaterim.cpp').split('bool FSFloaterIM::sendNativeMessage(', 1)[1].split('void FSFloaterIM::sendMsg(', 1)[0]
        for guard in ['mSessionInitialized', 'mTextIMPossible', 'mInputEditor->getEnabled()', 'mModeratorMutedText', 'FSData::NO_SUPPORT', 'gDisconnected']:
            self.assertIn(guard, send)
        self.assertIn('sendMsg(', send)
        self.assertNotIn('mInputEditor->setText', send)
        self.assertNotIn('mQueuedMsgsForInit', send)

    def test_im_lifetimes_and_targeted_drafts(self):
        cpp = source('llimview.cpp')
        clear = cpp.split('bool LLIMModel::clearSession(', 1)[1].split('void LLIMModel::getMessages(', 1)[0]
        self.assertLess(clear.index('FSNativeChat::removeIM('), clear.index('delete ('))
        self.assertIn('FSNativeChat::rekeyIM(old_session_id, new_session_id)', cpp)
        native = source('fsnativechatwin32.cpp')
        self.assertIn('mBook.matches(command.conversation, command.token)', native)
        self.assertIn('mBridge->conversation != command.conversation || mBridge->token != command.token', native)
        self.assertIn('mBook.clear()', native)
        self.assertIn('mPending || mSwitching', native)
        ui = native[native.index('class NativeWindow'):native.index('// VIEWER MAIN THREAD ONLY.')]
        for forbidden in ['LLIMModel::', 'FSFloaterIM::', 'LLMuteList::']:
            self.assertNotIn(forbidden, ui)

    def test_toybox_command_and_labels(self):
        commands = ET.fromstring(source('app_settings/commands.xml'))
        names = [command.get('name') for command in commands.findall('command')]
        self.assertEqual(len(names), len(set(names)))
        command = commands.find("command[@name='native_chat']")
        self.assertEqual(command.get('available_in_toybox'), 'true')
        self.assertEqual(command.get('execute_function'), 'NativeChat.Show')
        self.assertEqual(command.get('is_enabled_function'), 'NativeChat.Supported')
        self.assertEqual(command.get('is_running_function'), 'NativeChat.IsOpen')
        menu = source('llviewermenu.cpp')
        self.assertIn('enable.add("NativeChat.IsOpen"', menu)
        for lang in ['en', 'de']:
            strings = ET.fromstring(source(f'skins/default/xui/{lang}/strings.xml'))
            for name in [command.get('label_ref'), command.get('tooltip_ref'), 'NativeChatLocal', 'NativeChatIM', 'NativeChatGroup']:
                self.assertIsNotNone(strings.find(f"string[@name='{name}']"))

    def test_windows_theme_is_read_only_and_live(self):
        theme = source('fsnativechattheme.h')
        for required in ['KEY_QUERY_VALUE', 'AppsUseLightTheme', 'SPI_GETHIGHCONTRAST', 'DWMWA_USE_IMMERSIVE_DARK_MODE', 'chooseTheme(', 'DeleteObject(']:
            self.assertIn(required, theme)
        for forbidden in ['RegSetValue', 'RegCreateKey', 'SPI_SETHIGHCONTRAST', 'SetSysColors', 'SetPreferredAppMode']:
            self.assertNotIn(forbidden, theme)
        cpp = source('fsnativechatwin32.cpp')
        for message in ['WM_THEMECHANGED', 'WM_SETTINGCHANGE', 'WM_SYSCOLORCHANGE', 'WM_DRAWITEM']:
            self.assertIn(message, cpp)
        self.assertIn('mTheme.richEdit(mHistory)', cpp)
        self.assertIn('comctl32 dwmapi', source('CMakeLists.txt'))

    def test_font_size_reapplies_current_rich_edit_colors(self):
        cpp = source('fsnativechatwin32.cpp')
        apply = cpp.split('void applyFont()', 1)[1].split('void applyTheme()', 1)[0]
        self.assertIn('mTheme.richEditFont(mHistory, font)', apply)
        self.assertNotIn('SendMessageW(mHistory, WM_SETFONT', apply)
        theme = source('fsnativechattheme.h')
        font = theme.split('void richEditFont(', 1)[1].split('HBRUSH colorControl(', 1)[0]
        self.assertLess(font.index('SendMessageW(control, WM_SETFONT'), font.index('richEdit(control)'))
        self.assertLess(font.index('richEdit(control)'), font.index('RedrawWindow('))
        self.assertNotIn('refresh(', font)
        palette = theme.split('void richEdit(HWND control)', 1)[1].split('void richEditFont(', 1)[0]
        for required in ['CFM_COLOR | CFM_BACKCOLOR', 'format.crTextColor = foreground', 'format.crBackColor = surface', 'SCF_DEFAULT', 'SCF_ALL']:
            self.assertIn(required, palette)

    def test_friend_open_is_checked_and_never_sends(self):
        cpp = source('fsnativechatwin32.cpp')
        opener = cpp.split('void openFriend(const Command& command)', 1)[1].split('bool canSendSelected()', 1)[0]
        for required in ['command.generation == mGeneration', 'mFriends.matches(', 'getBuddyInfo(id)', 'RlvActions::canStartIM(id)', 'Book::MaxConversations', 'LLIMMgr::instance().addSession(']:
            self.assertIn(required, opener)
        for forbidden in ['sendMsg(', 'sendNativeMessage(', 'LLIMModel::sendMessage(', 'LLAvatarActions::startIM(']:
            self.assertNotIn(forbidden, opener)
        ui = cpp[cpp.index('class NativeWindow'):cpp.index('// VIEWER MAIN THREAD ONLY.')]
        self.assertNotIn('LLAvatarTracker::', ui)
        self.assertNotIn('LLAvatarNameCache::', ui)
        for required in ['FriendSearchID', 'LBN_DBLCLK', 'Command::OpenFriend', 'FindNLSStringEx(', 'mFriends.clear()']:
            self.assertIn(required, cpp)

    def test_tabs_and_per_conversation_autoscroll(self):
        cpp = source('fsnativechatwin32.cpp')
        self.assertIn('child(WC_TABCONTROLW', cpp)
        self.assertNotIn('mConversations, CB_', cpp)
        for required in ['TCN_SELCHANGING', 'TCN_SELCHANGE', 'mTabs.selected(', 'mViews.clear()', 'mTabs.contains(', 'currentView().remember(mHistory)', 'AutoScrollID', 'LatestID', 'historyCount', 'historyDropped']:
            self.assertIn(required, cpp)
        self.assertIn('ODT_TAB', source('fsnativechattheme.h'))
        for lang in ['en', 'de']:
            strings = ET.fromstring(source(f'skins/default/xui/{lang}/strings.xml'))
            for name in ['NativeChatAutoScroll', 'NativeChatLatest']:
                self.assertIsNotNone(strings.find(f"string[@name='{name}']"))

    def test_platform_and_settings(self):
        cmake = source('CMakeLists.txt')
        windows = cmake[cmake.index('if (WINDOWS)'):cmake.index('endif (WINDOWS)')]
        self.assertIn('fsnativechatwin32.cpp', windows)
        self.assertIn('fsnativechat.h', cmake)
        self.assertIn('comctl32', cmake)
        self.assertIn('inline bool supported() { return false; }', source('fsnativechat.h'))
        children = list(ET.fromstring(source('app_settings/settings.xml')).find('map'))
        keys = [children[i].text for i in range(0, len(children), 2)]
        self.assertEqual(len(keys), len(set(keys)))
        for name in ['FSNativeChatRect', 'FSNativeChatFontSize', 'FSNativeChatTopmost']:
            self.assertIn(name, keys)
        # A draft or chat transcript must never become a saved setting.
        self.assertFalse(any(name in keys for name in ['FSNativeChatDraft','FSNativeChatHistory']))

if __name__ == '__main__': unittest.main()
