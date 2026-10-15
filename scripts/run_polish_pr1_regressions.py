"""Fresh native QA cases for the PR #1 polish build (2026-10-13).
Same isolation as run_full_redesign_regressions.py: private desktop, QA package only,
serial runs, and each previous QA profile is moved aside, never overwritten.
Light cases seed only the fresh QA profile with color-scheme-index=1 (Light).
"""
from pathlib import Path
import os, subprocess, json, shutil, sys
work = Path(__file__).resolve().parent
package = work/'portable-full-video-qa'
base = [
 ('settings-profiles', 'music-long.mp3', {'VLC_NATIVE_SAVE_HISTORY': '1'}),
 ('full-video', 'fixture.mp4', {'VLC_REDESIGN_QA_FULLVIDEO': '1', 'VLC_NATIVE_RENDER_VIDEO': '1', 'VLC_NATIVE_NO_HISTORY': '1'}),
 ('toolbar', 'fixture.mp4', {'VLC_NATIVE_TOOLBAR': '1', 'VLC_NATIVE_NO_HISTORY': '1'}),
 ('feature-parity', 'fixture-chapters.mkv', {'VLC_NATIVE_PARITY': '1', 'VLC_NATIVE_NO_HISTORY': '1'}),
 ('layout', 'fixture.mp4', {'VLC_NATIVE_LAYOUT': '1', 'VLC_NATIVE_RENDER_VIDEO': '1', 'VLC_NATIVE_NO_HISTORY': '1'})]
prefix = sys.argv[1] if len(sys.argv) > 1 else 'p1'
only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
cases = [(f'{prefix}-{name}-dark', fixture, flags, None) for name, fixture, flags in base]
cases += [(f'{prefix}-{name}-light', fixture, flags, 1) for name, fixture, flags in base if name != 'toolbar']
summary = []
for case, fixture, flags, scheme in cases:
    if only and case not in only: continue
    if (work/'qa'/case).exists(): raise ValueError(f'QA case {case} already exists')
    profile = package/'portable'; preserved = work/'qa-profile-backups'/case
    if preserved.exists(): raise ValueError('Do not overwrite previous QA state')
    preserved.parent.mkdir(exist_ok=True)
    if profile.exists(): shutil.move(str(profile), str(preserved))
    profile.mkdir()
    if scheme is not None:
        (profile/'vlc').mkdir()
        (profile/'vlc'/'vlc-qt-interface.ini').write_text(f'[MainWindow]\ncolor-scheme-index={scheme}\n', encoding='utf-8')
    environment = {k: v for k, v in os.environ.items() if not k.startswith(('VLC_NATIVE_', 'VLC_REDESIGN_QA_'))}
    environment.update(VLC_NATIVE_PACKAGE_PATH=str(package), VLC_NATIVE_QA_CASE=case,
        VLC_NATIVE_QA_FIXTURE=fixture, VLC_REDESIGN_QA_PLAYER_FIRST='1', **flags)
    if scheme is not None: environment['VLC_REDESIGN_QA_SCHEME'] = str(scheme)
    print('Starting '+case, flush=True)
    run = subprocess.run(['python', str(work/'check_native_interface.py')], env=environment)
    result = {'case': case, 'runnerExit': run.returncode}
    try:
        checks = json.loads((work/'qa'/case/'checks.json').read_text())
        process = json.loads((work/'qa'/case/'process-report.json').read_text())
        result.update(booleanChecks=sum(isinstance(v, bool) for v in checks.values()),
            failures=[k for k, v in checks.items() if v is False],
            qmlWarnings=checks.get('qmlWarnings'), exitCode=process['exitCode'],
            screenshots=len(list((work/'qa'/case).glob('*.png'))))
    except Exception as error:
        result['error'] = repr(error)
    print(json.dumps(result), flush=True)
    summary.append(result)
(work/'qa'/f'{prefix}-summary.json').write_text(json.dumps(summary, indent=2))
print('SUMMARY_WRITTEN', flush=True)
