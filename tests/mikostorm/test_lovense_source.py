"""Focused Lovense wiring checks; no network, viewer or MCP tests."""
from pathlib import Path
import ast
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / 'indra/newview'


class LovenseIntegration(unittest.TestCase):
    def test_settings_are_global_and_disabled(self):
        children = list(ET.parse(VIEW / 'app_settings/settings.xml').getroot().find('map'))
        settings = {children[i].text: children[i + 1] for i in range(0, len(children), 2)}
        fields = list(settings['MikoLovenseEnabled'])
        values = {fields[i].text: fields[i + 1].text for i in range(0, len(fields), 2)}
        self.assertEqual(values['Value'], '0')
        for name in ['MikoLovensePlatform', 'MikoLovensePattern', 'MikoLovenseMaxStrength', 'MikoLovenseShowHelp']:
            self.assertIn(name, settings)
        self.assertFalse(any('Lovense' in n and ('Token' in n or 'Secret' in n) for n in settings))

    def test_localized_controls_match_and_fit(self):
        roots = [ET.parse(VIEW / f'skins/default/xui/{lang}/floater_miko_lovense.xml').getroot()
                 for lang in ['en', 'de']]
        controls = [{(e.tag, e.get('name')) for e in root if e.get('name')} for root in roots]
        self.assertEqual(*controls)
        for root in roots:
            self.assertEqual(root.get('save_visibility'), 'false')
            for element in root:
                if element.get('left') is not None:
                    self.assertGreaterEqual(int(element.get('left')), 0)
                    self.assertLessEqual(int(element.get('left')) + int(element.get('width')), int(root.get('width')))
                    self.assertLessEqual(int(element.get('top')) + int(element.get('height')), int(root.get('height')))
            token = root.find("line_editor[@name='auth_token']")
            self.assertEqual(token.get('is_password'), 'true')
            self.assertIsNone(token.get('control_name'))

    def test_toolbar_and_menu(self):
        command = ET.parse(VIEW / 'app_settings/commands.xml').getroot().find("command[@name='miko_lovense']")
        self.assertEqual(command.get('available_in_toybox'), 'true')
        self.assertEqual(command.get('execute_parameters'), 'miko_lovense')
        menu = ET.parse(VIEW / 'skins/default/xui/en/menu_viewer.xml').getroot().find(
            ".//menu_item_call[@name='Miko Lovense']/menu_item_call.on_click")
        self.assertEqual(menu.get('parameter'), 'miko_lovense')
        self.assertIn('build<FSFloaterLovense>', (VIEW / 'llviewerfloaterreg.cpp').read_text())

    def test_lifecycle_stops_before_teardown(self):
        app = (VIEW / 'llappviewer.cpp').read_text()
        for method in ['sendLogoutRequest', 'disconnectViewer']:
            body = app.split('void LLAppViewer::' + method + '()')[1].split('\n}', 1)[0]
            self.assertIn('MikoLovense::Client::instance().disconnect()', body)
        cleanup = app.split('bool LLAppViewer::cleanup()')[1]
        self.assertLess(cleanup.index('MikoLovense::Client::instance().shutdown()'), cleanup.index('mAppCoreHttp.cleanup()'))
        ui = (VIEW / 'lovense/fsfloaterlovense.cpp').read_text()
        self.assertRegex(ui, r'disconnect\(\);\s*LLFloater::onClose')

    def test_no_foreign_binary_dependencies(self):
        cmake = (VIEW / 'CMakeLists.txt').read_text()
        for name in ['lovense/fslovense.cpp', 'lovense/fsfloaterlovense.cpp', 'lovense/QrCode.cpp']:
            self.assertIn(name, cmake)
        self.assertNotIn('sioclient.lib', cmake)
        self.assertFalse(any((VIEW / 'lovense').rglob('*.dll')))
        self.assertFalse(any((VIEW / 'lovense').rglob('*.lib')))

    def test_credentials_and_verified_tls(self):
        script = (ROOT / 'scripts/lovense_session_token.py').read_text(encoding='utf-8-sig')
        ast.parse(script)
        self.assertIn('getpass.getpass', script)
        self.assertIn('ssl.create_default_context()', script)
        transport = (VIEW / 'lovense/fslovense.cpp').read_text(encoding='utf-8-sig')
        self.assertNotIn('LL_INFOS', transport)
        self.assertNotIn('SDK_DEV_TOKEN', transport)
        self.assertRegex(transport, r'CURLOPT_SSL_VERIFYPEER,\s*1L')
        self.assertIn('ssl::host_name_verification', transport)


if __name__ == '__main__':
    unittest.main()
