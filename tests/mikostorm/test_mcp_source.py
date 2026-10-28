"""Static integration checks. These do not compile or run the viewer."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
VIEWER = ROOT / 'indra/newview'
def source(name): return (VIEWER / name).read_text(encoding='utf-8-sig')

class MCPSource(unittest.TestCase):
    def test_declared_controls_and_localization(self):
        en = ET.fromstring(source('skins/default/xui/en/floater_mcp_control.xml'))
        de = ET.fromstring(source('skins/default/xui/de/floater_mcp_control.xml'))
        names = {e.get('name') for e in en.iter() if e.get('name')}
        self.assertEqual(len(names), sum(bool(e.get('name')) for e in en.iter()))
        used = set(re.findall(r'getChild<[^>]+>\("([^"]+)"\)', source('llfloatermcp.cpp')))
        used.update(re.findall(r'getString\("([^"]+)"\)', source('llfloatermcp.cpp')))
        self.assertTrue(used <= names, used - names)
        self.assertTrue({e.get('name') for e in de.iter() if e.get('name')} <= names)
        for control in en:
            if 'top' not in control.attrib: continue
            self.assertLessEqual(int(control.get('top')) + int(control.get('height')), int(en.get('height')))
            self.assertLessEqual(int(control.get('left')) + int(control.get('width')), int(en.get('width')))
        for name in ['global_token', 'new_token']:
            self.assertEqual(en.find(f"line_editor[@name='{name}']").get('is_password'), 'true')

    def test_preferences_and_floater_registration(self):
        for lang in ['en', 'de']:
            panel = ET.fromstring(source(f'skins/default/xui/{lang}/panel_preferences_setup.xml'))
            self.assertIsNotNone(panel.find(".//panel[@name='tab-mcp']/button[@name='mcp_control']"))
        self.assertIn('LLFloaterReg::add("mcp_control", "floater_mcp_control.xml"', source('llviewerfloaterreg.cpp'))
        self.assertIn('LLMCPServer::configureLive()', source('llviewercontrol.cpp'))

    def test_mcp_settings_unique_and_typed(self):
        tree = ET.fromstring(source('app_settings/settings.xml'))
        children = list(tree.find('map'))
        keys = [children[i].text for i in range(0, len(children), 2)]
        self.assertEqual(len(keys), len(set(keys)))
        settings = dict(zip(keys, children[1::2]))
        for key, kind in {'MCPEnabled': 'Boolean', 'MCPPort': 'U32', 'MCPAuthToken': 'String',
                          'MCPDefaultPermissions': 'U32', 'MCPMediaPermissions': 'U32', 'MCPClients': 'LLSD'}.items():
            fields = list(settings[key]); values = {fields[i].text: fields[i+1] for i in range(0, len(fields), 2)}
            self.assertEqual(values['Type'].text, kind)
        self.assertIn('RAND_bytes', source('llmcpservice.cpp'))
        self.assertIn('CRYPTO_memcmp', source('llmcpservice.cpp'))
        self.assertNotIn('setDefaultLevel', source('llmcpserver.cpp'))

    def test_sources_and_headers_are_available(self):
        cmake = source('CMakeLists.txt')
        files = ['llmcpservice.cpp', 'llmcpoperations.cpp', 'llmcpviewertools.cpp', 'llfloatermcp.cpp',
                 'llmcpvalidation.h', 'llmcpcontext.h', 'llfloatermcp.h', 'llmcptask.h']
        for name in files:
            self.assertTrue((VIEWER/name).is_file()); self.assertIn(name, cmake)
        self.assertIn('LL_ADD_INTEGRATION_TEST(llmcpvalidation', cmake)
        self.assertTrue((VIEWER/'tests/llmcpvalidation_test.cpp').is_file())
        header_names = {p.name for p in (ROOT/'indra').rglob('*.h')}
        for name in files:
            for include in re.findall(r'^#include "([^"/]+)"', source(name), re.M):
                self.assertIn(include, header_names, (name, include))

    def test_protocol_and_queue_wiring(self):
        wire = source('llmcphttp.cpp'); server = source('llmcpserver.cpp'); service = source('llmcpservice.cpp')
        self.assertIn('request.path == "/health"', wire)
        self.assertNotIn('text/event-stream', wire)
        self.assertIn('MCP-Session-Id:', wire)
        self.assertIn('result.as_object()["id"] = id ? *id', wire)
        self.assertIn('context.cancelKey = boost::json::serialize(*target)', wire)
        self.assertIn('LLMCPValidation::validate(args, tool.input_schema', service)
        self.assertIn('return dispatchTool(name, args);', server)
        self.assertIn('context.active', server)
        self.assertLess(server.index('mSession->store(false)'), server.index('LLMCPHttpServer::stop()'))
        self.assertIn('sourceVolume(root_id, volume)', source('llpositionalstreammgr.cpp'))
        self.assertIn('sourceVolume(id, master_vol)', source('llpositionalstreammgr.cpp'))

if __name__ == '__main__': unittest.main()
