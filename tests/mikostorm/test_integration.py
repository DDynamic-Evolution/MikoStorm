from pathlib import Path
import json
import unittest
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
def source(name): return (ROOT / 'indra/newview' / name).read_text(encoding='utf-8-sig')
def body(text, signature):
    start = text.index('{', text.index(signature))
    depth = 1
    for end in range(start + 1, len(text)):
        depth += (text[end] == '{') - (text[end] == '}')
        if not depth: return text[start:end + 1]
    raise AssertionError('unterminated function')
class Integration(unittest.TestCase):
    def test_texture_write_paths_recheck_permissions(self):
        text = source('llpreviewtexture.cpp')
        signatures = ['void LLPreviewTexture::saveTextureToFile(const std::vector<std::string>& filenames, EFileformatType', 'void LLPreviewTexture::saveMultipleToFile(const std::string& file_name']
        for signature in signatures: self.assertIn('canSaveAs()', body(text, signature))
        for signature in ['void LLPreviewTexture::onFileLoadedForSavePNG(', 'void LLPreviewTexture::onFileLoadedForSaveTGA(']:
            fn = body(text, signature)
            self.assertIn('self->hasExportPermission()', fn)
            self.assertLess(fn.index('self->hasExportPermission()'), fn.index('->save('))
    def test_removed_exporters(self):
        for name in ['pvassets.cpp','pvassets.h','fsmikoassets.cpp','fsmikoassets.h','fsmikoexportfloater.cpp','fsmikoexportfloater.h']:
            self.assertFalse((ROOT / 'indra/newview' / name).exists())
            self.assertNotIn(name, source('CMakeLists.txt'))
        for lang in ['en','de']:
            folder=ROOT / 'indra/newview/skins/default/xui' / lang
            self.assertFalse((folder / 'floater_miko_export.xml').exists())
            for path in folder.glob('menu*.xml'):
                self.assertNotIn('Object.ExportXML', path.read_text(encoding='utf-8-sig'))
                self.assertNotIn('Avatar.ExportXML', path.read_text(encoding='utf-8-sig'))
    def test_shared_numeric_camera_binding(self):
        for file in ['llfloatercamera.cpp','llfloaterpreference.cpp']:
            self.assertIn('mJointSelector.bind(findChild<LLComboBox>("joint_combo"))', source(file))
            self.assertNotIn('joint_combo->add(joint.first, joint.first)', source(file))
        self.assertIn('mCombo->add(joint.first, joint.second)', source('llcamerajointselector.cpp'))
        for lang in ['en','de']: ET.parse(ROOT / 'indra/newview/skins/default/xui' / lang / 'floater_camera.xml')
    def test_legacy_features(self):
        presets=json.loads((ROOT / 'indra/CMakePresets.json').read_text())['configurePresets']
        common=presets[0]['cacheVariables']
        for key in ['OPENSIM','USE_MCP','USE_3D_STREAM','USE_FMODSTUDIO']: self.assertEqual(common[key],'ON')
        self.assertEqual(common['USE_AVX_OPTIMIZATION'],'OFF')
        self.assertEqual(presets[2]['cacheVariables']['USE_AVX2_OPTIMIZATION'],'OFF')
        self.assertNotIn('/arch:AVX', common['LL_BUILD_ENV'])
    def test_live_asset_policy(self):
        fn=body(source('fsexportperms.cpp'),'bool FSExportPermsCheck::canExportAsset(')
        self.assertIn('exportable = allowed(grid, policy',fn)
        self.assertIn('if (!name)',fn)
        self.assertIn('if (!description)',fn)
if __name__ == '__main__': unittest.main()
