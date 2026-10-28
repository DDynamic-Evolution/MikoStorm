"""Verify the installer payload against the configured release and permission-aware UI."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import xml.etree.ElementTree as ET

def digest(path):
    checksum=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024),b''): checksum.update(block)
    return checksum.hexdigest()

def verify(root, build, installer, sevenzip):
    def run7(*arguments):
        result=subprocess.run([str(sevenzip),*map(str,arguments)],capture_output=True,encoding='utf-8',errors='replace')
        if result.returncode: raise RuntimeError(result.stdout[-3000:]+result.stderr[-3000:])
        return result.stdout
    folder=build/'verify-package'
    folder.mkdir(exist_ok=True)
    (folder/'integrity.log').write_text(run7('t',installer),encoding='utf-8')
    listing=run7('l','-slt',installer)
    (folder/'files.log').write_text(listing,encoding='utf-8')
    paths=[line[7:] for line in listing.splitlines() if line.startswith('Path = ')]
    if any(path.replace('\\','/').lower().endswith('floater_miko_export.xml') for path in paths):
        raise RuntimeError('Removed exporter UI is present in installer')
    release=build/'newview/Release'
    wanted={name:release/name for name in ['mikostorm.exe','fmod.dll','slplugin.exe','llplugin/media_plugin_cef.dll']}
    wanted['app_settings/settings.xml']=root/'indra/newview/app_settings/settings.xml'
    wanted['app_settings/commands.xml']=root/'indra/newview/app_settings/commands.xml'
    for language in ['en','de']:
        for name in ['floater_camera.xml','floater_animation_explorer.xml','floater_NACL_explore_sounds.xml',
                     'floater_mcp_control.xml','floater_fs_nearby_chat.xml','panel_preferences_setup.xml','strings.xml','notifications.xml']:
            relative=f'skins/default/xui/{language}/{name}'
            wanted[relative]=root/'indra/newview'/relative
        for path in (root/f'indra/newview/skins/default/xui/{language}').glob('menu*.xml'):
            relative=path.relative_to(root/'indra/newview').as_posix()
            wanted[relative]=path
    selected={}
    for target in [*wanted,'build_data.json']:
        matches=[path for path in paths if path.replace('\\','/').lower()==target.lower() or path.replace('\\','/').lower().endswith('/'+target.lower())]
        if len(matches)!=1: raise RuntimeError(f'Missing/ambiguous installer file: {target}')
        relative=Path(matches[0].replace('\\','/'))
        if relative.is_absolute() or '..' in relative.parts or ':' in str(relative): raise RuntimeError('Unsafe installer path')
        selected[target]=matches[0]
    run7('x','-y','-aoa','-o'+str(folder),installer,*selected.values())
    def extracted(name): return folder/selected[name].replace('\\','/')
    for name,source in wanted.items():
        if digest(extracted(name))!=digest(source): raise RuntimeError(f'Installer differs from release/source: {name}')
        if name.endswith('.xml'):
            text=extracted(name).read_text(encoding='utf-8-sig')
            if any(binding in text for binding in ['Object.ExportXML','Avatar.ExportXML','EnableExportXML']):
                raise RuntimeError(f'Removed exporter binding present: {name}')
    command=ET.parse(extracted('app_settings/commands.xml')).getroot().find("command[@name='native_chat']")
    if command is None or command.get('available_in_toybox') != 'true' or command.get('execute_function') != 'NativeChat.Show':
        raise RuntimeError('Native chat Toybox command missing or invalid')
    metadata=json.loads(extracted('build_data.json').read_text())
    version=(root/'indra/newview/VIEWER_VERSION_FS.txt').read_text().strip()+'.0'
    if metadata['Version']!=version or metadata['Address Size']!=64: raise RuntimeError('Wrong installer version/architecture')
    binary=extracted('mikostorm.exe').read_bytes()
    offset=struct.unpack_from('<I',binary,0x3c)[0]
    if struct.unpack_from('<H',binary,offset+4)[0]!=0x8664: raise RuntimeError('Viewer executable is not x64')
    for token in ['miko_export','Object.ExportXML','Avatar.ExportXML','MikoSaveAsset','LOSaveAsset','MikoExportFloater']:
        if token.encode() in binary or token.encode('utf-16le') in binary: raise RuntimeError(f'Removed exporter code present: {token}')
    for marker in ['FSNativeChatRect', 'NativeChat.Show', 'camera_list_joints', 'operation_status', 'NativeChat.IsOpen', 'NativeChatGroup', 'NativeChatFriends', 'NativeChatOpenFriend', 'AppsUseLightTheme', 'DwmSetWindowAttribute']:
        if marker.encode() not in binary and marker.encode('utf-16le') not in binary: raise RuntimeError(f'New release feature missing in executable: {marker}')
    pairs=list(ET.parse(extracted('app_settings/settings.xml')).getroot()[0])
    for index in range(0,len(pairs),2):
        if pairs[index].text=='FSDisableEmbeddedBrowser':
            fields=list(pairs[index+1])
            values={fields[i].text:fields[i+1].text for i in range(0,len(fields),2)}
            if values['Value']!='0': raise RuntimeError('Embedded browser is disabled by default')
            break
    else: raise RuntimeError('Embedded browser default is missing')
    report={'version':version,'installer':str(installer),'sha256':digest(installer),'verified_files':len(wanted),'browser_enabled':True,'removed_exporters_absent':True,'mcp_and_native_chat_present':True}
    (folder/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('Installer payload verified:',json.dumps(report),flush=True)
    return report
