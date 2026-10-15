"""Run this project's instrumented interface test on a private Windows desktop.
No input, capture, or focus operations target the user's desktop or applications.
"""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import os, subprocess, time, msvcrt, json, uuid

root=Path(__file__).resolve().parent
package=Path(os.environ.get('VLC_NATIVE_PACKAGE_PATH',str(root/'portable-test'))).resolve()
if not package.is_relative_to(root.parent): raise ValueError('QA package must be inside this task workspace')
case=os.environ.get('VLC_NATIVE_QA_CASE','native-interface-v1')
timeout=int(os.environ.get('VLC_NATIVE_TIMEOUT','90'))
if not 10 <= timeout <= 90: raise ValueError('QA timeout must be 10 to 90 seconds')
destination=root/'qa'/case
destination.mkdir(parents=True,exist_ok=True)
user=c.WinDLL('user32',use_last_error=True)
kernel=c.WinDLL('kernel32',use_last_error=True)
user.CreateDesktopW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p]
user.CreateDesktopW.restype=w.HANDLE
user.CloseDesktop.argtypes=[w.HANDLE]
user.CloseDesktop.restype=w.BOOL
class Startup(c.Structure):
    _fields_=[('cb',w.DWORD),('reserved',w.LPWSTR),('desktop',w.LPWSTR),('title',w.LPWSTR),('x',w.DWORD),('y',w.DWORD),('width',w.DWORD),('height',w.DWORD),('xchars',w.DWORD),('ychars',w.DWORD),('fill',w.DWORD),('flags',w.DWORD),('show',w.WORD),('reservedSize',w.WORD),('reservedData',c.c_void_p),('stdin',w.HANDLE),('stdout',w.HANDLE),('stderr',w.HANDLE)]
class Process(c.Structure):
    _fields_=[('process',w.HANDLE),('thread',w.HANDLE),('pid',w.DWORD),('tid',w.DWORD)]
kernel.CreateProcessW.argtypes=[w.LPCWSTR,w.LPWSTR,c.c_void_p,c.c_void_p,w.BOOL,w.DWORD,c.c_void_p,w.LPCWSTR,c.POINTER(Startup),c.POINTER(Process)]
kernel.CreateProcessW.restype=w.BOOL
kernel.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]; kernel.WaitForSingleObject.restype=w.DWORD
kernel.GetExitCodeProcess.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]; kernel.GetExitCodeProcess.restype=w.BOOL
kernel.TerminateProcess.argtypes=[w.HANDLE,w.UINT]; kernel.TerminateProcess.restype=w.BOOL
kernel.CloseHandle.argtypes=[w.HANDLE]; kernel.CloseHandle.restype=w.BOOL
desktop_name='VlcRedesignQA-'+uuid.uuid4().hex
# Create/read/write window objects only. No permission or call to switch desktops.
desktop=user.CreateDesktopW(desktop_name,None,None,0,0xC3,None)
if not desktop: raise c.WinError(c.get_last_error())
arguments=[str(package/'vlc.exe'),'--qt-compositor=dummy','--aout=dummy','--vout=dummy','--no-hw-dec','--file-caching=50','--no-video-title-show','--restore-playback-pos=0','--no-qt-privacy-ask','--no-qt-updates-notif','--no-metadata-network-access','--qt-pin-controls','--no-qt-video-autoresize','--no-one-instance','--no-crashdump','--verbose=2']
if os.environ.get('VLC_NATIVE_RENDER_VIDEO')=='1':
    arguments=[argument for argument in arguments if argument not in ['--qt-compositor=dummy','--vout=dummy']]
    arguments+=['--qt-compositor=auto','--vout=direct3d11']
if os.environ.get('VLC_NATIVE_NO_HISTORY')=='1': arguments.append('--no-save-recentplay')
elif os.environ.get('VLC_NATIVE_SAVE_HISTORY')=='1': arguments.append('--save-recentplay')
fixture=os.environ.get('VLC_NATIVE_QA_FIXTURE','music-long.mp3')
os.environ.update(VLC_REDESIGN_QA_DIR=str(destination),VLC_REDESIGN_QA_MEDIA='' if fixture=='none' else str(root/'qa'/fixture))
if os.environ.get('VLC_NATIVE_TIMELINE')=='1': os.environ['VLC_REDESIGN_QA_TIMELINE']='1'
else: os.environ.pop('VLC_REDESIGN_QA_TIMELINE',None)
if os.environ.get('VLC_NATIVE_TIMELINE_EXTERNAL')=='1': os.environ['VLC_REDESIGN_QA_TIMELINE_EXTERNAL']='1'
else: os.environ.pop('VLC_REDESIGN_QA_TIMELINE_EXTERNAL',None)
if os.environ.get('VLC_NATIVE_CONTROLS')=='1': os.environ['VLC_REDESIGN_QA_CONTROLS']='1'
else: os.environ.pop('VLC_REDESIGN_QA_CONTROLS',None)
if os.environ.get('VLC_NATIVE_TOOLBAR')=='1': os.environ['VLC_REDESIGN_QA_TOOLBAR']='1'
else: os.environ.pop('VLC_REDESIGN_QA_TOOLBAR',None)
if os.environ.get('VLC_NATIVE_PARITY')=='1': os.environ['VLC_REDESIGN_QA_PARITY']='1'
else: os.environ.pop('VLC_REDESIGN_QA_PARITY',None)
if os.environ.get('VLC_NATIVE_FIRST_RUN')=='1':
    arguments.remove('--no-qt-privacy-ask')
    os.environ['VLC_REDESIGN_QA_FIRST_RUN']='1'
else: os.environ.pop('VLC_REDESIGN_QA_FIRST_RUN',None)
if os.environ.get('VLC_NATIVE_SEEK_PREVIEW')=='1': os.environ['VLC_REDESIGN_QA_SEEK_PREVIEW']='1'
else: os.environ.pop('VLC_REDESIGN_QA_SEEK_PREVIEW',None)
if os.environ.get('VLC_NATIVE_LAYOUT')=='1': os.environ['VLC_REDESIGN_QA_LAYOUT']='1'
else: os.environ.pop('VLC_REDESIGN_QA_LAYOUT',None)
if os.environ.get('VLC_NATIVE_SMALL_WINDOW')=='1': os.environ['VLC_REDESIGN_QA_SMALL_WINDOW']='1'
else: os.environ.pop('VLC_REDESIGN_QA_SMALL_WINDOW',None)
if os.environ.get('VLC_NATIVE_SOFTWARE') == '1': os.environ['QT_QUICK_BACKEND']='software'
else: os.environ.pop('QT_QUICK_BACKEND',None)
if os.environ.get('VLC_NATIVE_INTERPRETER')=='1': os.environ['QV4_FORCE_INTERPRETER']='1'
information=Process()
fixture_server=None
if os.environ.get('VLC_NATIVE_PROVIDER_FIXTURE')=='1':
    from subtitle_provider_fixture import start_fixture
    fixture_server=start_fixture()
    os.environ['VLC_REDESIGN_QA_SUBTITLE_API']=f'http://127.0.0.1:{fixture_server.server_port}/api/v1/'
try:
    with (destination/'native-process.log').open('wb') as log, open(os.devnull,'rb') as input_file:
        handles=[msvcrt.get_osfhandle(input_file.fileno()),msvcrt.get_osfhandle(log.fileno())]
        for handle in handles: os.set_handle_inheritable(handle,True)
        startup=Startup(); startup.cb=c.sizeof(startup); startup.desktop=desktop_name
        startup.flags=0x100; startup.stdin=handles[0]; startup.stdout=handles[1]; startup.stderr=handles[1]
        command=c.create_unicode_buffer(subprocess.list2cmdline(arguments))
        if not kernel.CreateProcessW(arguments[0],command,None,None,True,0,None,str(package),c.byref(startup),c.byref(information)): raise c.WinError(c.get_last_error())
        start=time.monotonic(); snapshot_done=False
        while kernel.WaitForSingleObject(information.process,500)==258:
            if time.monotonic()-start>50 and not snapshot_done:
                from snapshot_test_threads import capture_threads
                capture_threads(information.process, information.pid, destination/'shutdown-threads.json')
                snapshot_done=True
            if time.monotonic()-start>timeout:
                from snapshot_test_threads import capture_threads
                capture_threads(information.process, information.pid, destination/'timeout-threads.json')
                kernel.TerminateProcess(information.process,124)
                raise TimeoutError(f'Only the isolated test process was stopped after {timeout} seconds')
        exit_code=w.DWORD(); kernel.GetExitCodeProcess(information.process,c.byref(exit_code))
        report={'exitCode':exit_code.value,'seconds':round(time.monotonic()-start,2),'checksPresent':(destination/'checks.json').is_file(),'privateDesktop':True}
        (destination/'process-report.json').write_text(json.dumps(report,indent=2))
        print(json.dumps(report))
finally:
    if fixture_server: fixture_server.shutdown(); fixture_server.server_close()
    if information.thread: kernel.CloseHandle(information.thread)
    if information.process: kernel.CloseHandle(information.process)
    user.CloseDesktop(desktop)
