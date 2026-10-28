"""Reproducible Windows profiles. Invoke through build-windows.cmd."""
from pathlib import Path
import argparse
from contextlib import contextmanager
import time
import json
import os
import runpy
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

def run(command, **kwargs):
    print('> ' + subprocess.list2cmdline([str(p) for p in command]), flush=True)
    subprocess.run([str(p) for p in command], check=True, **kwargs)

def cache_values(path):
    result = {}
    if path.exists():
        for line in path.read_text(encoding='utf-8').splitlines():
            if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
                key, value = line.split('=', 1)
                result[key.split(':', 1)[0]] = value
    return result

def verify_profile(build, profile):
    values = cache_values(build / 'CMakeCache.txt')
    expected = {'OPENSIM':'ON','USE_FMODSTUDIO':'ON','USE_3D_STREAM':'ON','USE_MCP':'ON',
                'USE_AVX_OPTIMIZATION':'OFF','USE_AVX2_OPTIMIZATION':'ON' if profile == 'avx2' else 'OFF',
                'USE_VELOPACK':'OFF','PACKAGE':'ON'}
    for key, value in expected.items():
        if values.get(key) != value: raise RuntimeError(f'{key} must be {value}, got {values.get(key)}')
    if Path(values['CMAKE_HOME_DIRECTORY']).resolve() != (ROOT / 'indra').resolve():
        raise RuntimeError('Build cache points at a different checkout')
    if profile == 'legacy':
        for project in build.rglob('*.vcxproj'):
            text = project.read_text(encoding='utf-8-sig').lower()
            definitions = set()
            for block in re.findall(r'<preprocessordefinitions[^>]*>(.*?)</preprocessordefinitions>', text):
                definitions.update(piece.split('=', 1)[0].strip() for piece in block.split(';'))
            if '/arch:avx' in text or 'advancedvectorextensions' in text or definitions.intersection({'use_avx_optimization', 'use_avx2_optimization'}):
                raise RuntimeError(f'AVX instructions enabled in legacy project: {project}')

def configure(profile):
    if profile == 'legacy':
        for key in ('CL', '_CL_'):
            if '/arch:avx' in os.environ.get(key, '').lower():
                raise RuntimeError(f'{key} injects AVX flags into the legacy compiler environment')
    build = ROOT / f'build-vc170-64-{profile}'
    values = cache_values(build / 'CMakeCache.txt')
    if values and (Path(values['CMAKE_HOME_DIRECTORY']).resolve() != (ROOT / 'indra').resolve() or
                   Path(values['CMAKE_CACHEFILE_DIR']).resolve() != build.resolve()):
        raise RuntimeError(f'{build} contains a copied cache. Use a fresh profile directory.')
    # Reuse only third-party packages, never generated projects/caches from another checkout.
    if not (build / 'packages/installed-packages.xml').exists():
        seed = next((p for p in (ROOT / 'build-vc170-64/packages', ROOT / 'build-vc170-64-noavx/packages')
                     if (p / 'installed-packages.xml').exists()), None)
        if seed:
            result = subprocess.run(['robocopy', str(seed), str(build / 'packages'), '/E', '/COPY:DAT', '/R:1', '/W:1', '/NFL', '/NDL', '/NP'])
            if result.returncode >= 8: raise RuntimeError('Could not copy dependency packages')
    env = os.environ.copy()
    # Profiles supply their own baseline; an inherited LL_BUILD must not inject AVX into legacy builds.
    env.pop('LL_BUILD', None)
    run(['cmake', '--preset', f'windows-{profile}'], cwd=ROOT / 'indra', env=env)
    verify_profile(build, profile)
    return build

@contextmanager
def test_lock():
    import msvcrt
    directory = ROOT / 'build-vc170-64-tests'
    directory.mkdir(exist_ok=True)
    with (directory / 'suite.lock').open('a+b') as lock:
        if lock.tell() == 0:
            lock.write(b'0')
            lock.flush()
        deadline = time.monotonic() + 180
        while True:
            lock.seek(0)
            try:
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                break
            except OSError:
                if time.monotonic() >= deadline: raise RuntimeError('Another build holds the regression suite lock')
                time.sleep(0.25)
        try: yield
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)

def tests():
    with test_lock(): run_tests()

def run_tests():
    build = ROOT / 'build-vc170-64-tests'
    run(['cmake', '-S', ROOT / 'tests/mikostorm', '-B', build, '-G', 'Visual Studio 17 2022', '-A', 'x64'])
    run(['cmake', '--build', build, '--config', 'Release', '--', '/m:1', '/nr:false'])
    run(['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure'])
    run([sys.executable, '-B', '-m', 'unittest', 'discover', '-s', ROOT / 'tests/mikostorm', '-p', 'test_*.py', '-v'])

def package(build, profile, prepare):
    project = ET.parse(build / 'newview/llpackage.vcxproj').getroot()
    ns = {'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
    commands = [e.text for e in project.findall('.//m:CustomBuild/m:Command', ns)
                if 'Release|x64' in e.get('Condition', '') and 'viewer_manifest.py' in (e.text or '')]
    if len(commands) != 1: raise RuntimeError('Could not find the Release packaging command')
    line = next(x.strip() for x in commands[0].splitlines() if 'viewer_manifest.py' in x)
    arguments = [x.strip('"') for x in shlex.split(line.replace('$(Configuration)', 'Release'), posix=False)]
    sys.path[:0] = [str(ROOT / 'indra/lib/python'), str(ROOT / 'indra/newview')]
    from indra.util.llmanifest import LLManifest
    original = LLManifest.run_command
    class Prepared(BaseException): pass
    def command(self, args, *rest, **kwargs):
        if prepare and isinstance(args, (tuple, list)) and Path(str(args[0])).name.lower() == 'makensis.exe':
            script = Path(args[-1]).resolve()
            if not script.exists(): raise RuntimeError('NSIS script is missing')
            (build / 'nsis-prepared.json').write_text(json.dumps({'script':str(script),'command':list(args)}, indent=2))
            print(f'NSIS script prepared: {script}', flush=True)
            raise Prepared()
        return original(self, args, *rest, **kwargs)
    LLManifest.run_command = command
    before = Path.cwd()
    old_argv = sys.argv
    try:
        os.chdir(build / 'newview')
        sys.argv = arguments[1:]
        try: runpy.run_path(arguments[1], run_name='__main__')
        except Prepared: return
    finally:
        os.chdir(before)
        sys.argv = old_argv
        LLManifest.run_command = original
    collect(build, profile)

def collect(build, profile):
    version = (ROOT / 'indra/newview/VIEWER_VERSION_FS.txt').read_text().strip()
    code = version.replace('.', '-')
    installers = list((build / 'newview/Release').glob(f'*{code}-*_Setup.exe'))
    if len(installers) != 1: raise RuntimeError(f'Expected one installer for {version}, got {installers}')
    installer = installers[0]
    from verify_windows_package import verify, digest
    verify(ROOT, build, installer, shutil.which('7z') or r'C:\Program Files\7-Zip\7z.exe')
    destination = ROOT / 'dist' / f'MikoStorm-{version}-{profile.upper()}-FMOD-OpenSim-3D-MCP-Setup.exe'
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(installer, destination)
    checksum = digest(destination)
    destination.with_suffix('.exe.sha256').write_text(f'{checksum}  {destination.name}\n')
    (destination.parent / f'{profile}-build.json').write_text(json.dumps({'version':version,'profile':profile,
        'sha256':checksum,'features':['FMOD','OpenSim','3D Stream','MCP'],'source':str(ROOT),
        'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True))},indent=2))
    print(destination, flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', choices=['avx2','legacy','all'], nargs='?', default='avx2')
    parser.add_argument('action', choices=['configure','build','package','prepare-package','collect','test'], nargs='?', default='build')
    args = parser.parse_args()
    if args.action == 'test': configure('avx2' if args.profile == 'all' else args.profile); tests(); return
    tested = False
    for profile in (['avx2','legacy'] if args.profile == 'all' else [args.profile]):
        if args.action == 'collect':
            build = ROOT / f'build-vc170-64-{profile}'
            verify_profile(build, profile)
            collect(build, profile)
            continue
        build = configure(profile)
        if args.action == 'configure': continue
        if not tested:
            tests()
            tested = True
        run(['MSBuild.exe', build / 'newview/mikostorm.vcxproj', '/t:Build', '/p:Configuration=Release',
             '/p:Platform=x64', '/m:1', '/nr:false', '/v:minimal', '/nologo', '/fl',
             f'/flp:LogFile={build / "viewer-build.log"};Verbosity=normal'])
        if args.action in ('package','prepare-package'): package(build,profile,args.action == 'prepare-package')
if __name__ == '__main__':
    try: main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Build failed: {error}', file=sys.stderr)
        sys.exit(1)
