"""Dependency and viewer/voice wiring checks; no compiler, viewer or MCP calls."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
def source(path): return (ROOT/path).read_text(encoding='utf-8-sig')
def mapping(element):
    children = list(element)
    keys = [child.text for child in children[::2]]
    if len(keys) != len(set(keys)): raise AssertionError('Duplicate LLSD keys')
    return {children[i].text: children[i+1] for i in range(0, len(children), 2)}
class UpstreamAudioIntegration(unittest.TestCase):
    def test_browser_packages_are_consistent_across_platforms(self):
        packages = mapping(mapping(ET.fromstring(source('autobuild.xml'))[0])['installables'])
        package = mapping(packages['dullahan'])
        version = package['version'].text
        self.assertTrue(version.startswith('1.50.0.'))
        self.assertIn('chromium-152.', version)
        for platform in ['windows64', 'darwin64', 'linux64']:
            archive = mapping(mapping(mapping(package['platforms'])[platform])['archive'])
            self.assertIn(version, archive['url'].text)
            self.assertRegex(archive['hash'].text, r'^[0-9a-f]{40}$')
    def test_voice_packages_and_windows_crypto_link(self):
        packages = mapping(mapping(ET.fromstring(source('autobuild.xml'))[0])['installables'])
        package = mapping(packages['webrtc'])
        version = package['version'].text
        self.assertTrue(version.startswith('m144.'))
        for platform in ['windows64', 'darwin64', 'linux64']:
            archive = mapping(mapping(mapping(package['platforms'])[platform])['archive'])
            self.assertIn(version, archive['url'].text)
            self.assertRegex(archive['hash'].text, r'^[0-9a-f]{40}$')
        self.assertIn('crypt32', source('indra/llwebrtc/CMakeLists.txt'))
        self.assertEqual(mapping(packages['fmodstudio'])['version'].text, '2.03.14')
    def test_new_mic_control_is_declared_once_and_wired(self):
        controls = mapping(ET.fromstring(source('indra/newview/app_settings/settings.xml'))[0])
        setting = mapping(controls['StopMicCaptureWhenMuted'])
        self.assertEqual(setting['Type'].text, 'S32')
        self.assertIn(setting['Value'].text, ['0', '1'])
        voice = source('indra/newview/llvoicewebrtc.cpp')
        self.assertIn('getControl("StopMicCaptureWhenMuted")', voice)
        self.assertIn('setStopCaptureOnMute(gSavedSettings.getS32("StopMicCaptureWhenMuted"))', voice)
        self.assertIn('mStopMicCaptureOnMuteConnection.disconnect()', voice)
    def test_interface_and_implementation_have_device_lifecycle_methods(self):
        interface = source('indra/llwebrtc/llwebrtc.h')
        impl = source('indra/llwebrtc/llwebrtc_impl.h')
        cpp = source('indra/llwebrtc/llwebrtc.cpp')
        for method in ['setVoiceEnabled', 'setStopCaptureOnMute']:
            self.assertIn(f'virtual void {method}(', interface)
            self.assertIn(f'void {method}(', impl)
            self.assertIn(f'void LLWebRTCImpl::{method}(', cpp)
        self.assertIn('workerDisableBuiltInAudioProcessing()', cpp)
        self.assertIn('mHaveDefaultPlayoutDeviceGuid', impl)
        self.assertIn('mHaveDefaultRecordingDeviceGuid', impl)
    def test_voice_shutdown_services_only_its_private_queue(self):
        voice = source('indra/newview/llvoicewebrtc.cpp')
        drain = voice.split('void LLWebRTCVoiceClient::drainConnections()', 1)[1].split('const LLVoiceVersionInfo& LLWebRTCVoiceClient::getVersion()', 1)[0]
        self.assertIn('mVoiceWorkQueue->runFor', drain)
        self.assertFalse('mMainQueue' in voice, 'Voice must not service the viewer main queue')
        self.assertNotIn('getInstance("mainloop")', voice)
        self.assertIn('DRAIN_TIMEOUT_SECONDS', drain)
        terminate = voice.split('void LLWebRTCVoiceClient::terminate()', 1)[1].split('void LLWebRTCVoiceClient::drainConnections()', 1)[0]
        self.assertLess(terminate.index('drainConnections()'), terminate.index('llwebrtc::terminate()'))
        self.assertIn('isWebRTCTerminated()', voice)
    def test_component_does_not_require_absent_stats_or_moderation_services(self):
        voice = source('indra/newview/llvoicewebrtc.cpp')
        for dependency in ['llnearbyvoicemoderation.h', 'LLNearbyVoiceModeration::', 'LLStatViewer::WEBRTC_', 'resetConnectionStats()', 'gatherConnectionStats()', 'updateVersion()']:
            self.assertNotIn(dependency, voice)
        self.assertIn('participant->mIsModeratorMuted = participant_obj["m"].as_bool()', voice)
        self.assertIn('std::string LLWebRTCVoiceClient::sipURIFromID', voice)
if __name__ == '__main__': unittest.main()
