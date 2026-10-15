from pathlib import Path
import os,subprocess,json,shutil
work=Path(__file__).resolve().parent
package=work/'portable-full-video-qa'
cases=[
 ('v99-settings-profiles-final-fixed','music-long.mp3',{'VLC_NATIVE_SAVE_HISTORY':'1'}),
 ('v100-full-video-final-fixed','fixture.mp4',{'VLC_REDESIGN_QA_FULLVIDEO':'1','VLC_NATIVE_RENDER_VIDEO':'1','VLC_NATIVE_NO_HISTORY':'1'}),
 ('v101-toolbar-final-fixed','fixture.mp4',{'VLC_NATIVE_TOOLBAR':'1','VLC_NATIVE_NO_HISTORY':'1'}),
 ('v102-feature-parity-final-fixed','fixture-chapters.mkv',{'VLC_NATIVE_PARITY':'1','VLC_NATIVE_NO_HISTORY':'1'}),
 ('v103-layout-final-fixed','fixture.mp4',{'VLC_NATIVE_LAYOUT':'1','VLC_NATIVE_RENDER_VIDEO':'1','VLC_NATIVE_NO_HISTORY':'1'})]
for case,fixture,flags in cases:
    profile=package/'portable';preserved=work/'qa-profile-backups'/case
    if preserved.exists():raise ValueError('Do not overwrite previous QA state')
    preserved.parent.mkdir(exist_ok=True)
    if profile.exists():shutil.move(str(profile),str(preserved))
    profile.mkdir()
    environment={k:v for k,v in os.environ.items() if not k.startswith(('VLC_NATIVE_','VLC_REDESIGN_QA_'))}
    environment.update(VLC_NATIVE_PACKAGE_PATH=str(package),VLC_NATIVE_QA_CASE=case,
        VLC_NATIVE_QA_FIXTURE=fixture,VLC_REDESIGN_QA_PLAYER_FIRST='1',**flags)
    print('Starting '+case,flush=True)
    subprocess.run(['python',str(work/'check_native_interface.py')],env=environment,check=True)
    checks=json.loads((work/'qa'/case/'checks.json').read_text())
    process=json.loads((work/'qa'/case/'process-report.json').read_text())
    failures=[k for k,v in checks.items() if v is False]
    print(json.dumps({'case':case,'booleanChecks':sum(isinstance(v,bool) for v in checks.values()),'failures':failures,'warnings':checks.get('qmlWarnings'),'exitCode':process['exitCode']}),flush=True)
    if failures or checks.get('qmlWarnings') or process['exitCode']!=0:raise RuntimeError('Native release regression failed')
print('ALL_FULL_REDESIGN_REGRESSIONS_PASSED',flush=True)
