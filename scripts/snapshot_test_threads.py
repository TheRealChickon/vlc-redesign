"""Read thread locations in our own development process after QA completes."""
import ctypes as c
from ctypes import wintypes as w
import json

def capture_threads(process, pid, destination):
    kernel=c.WinDLL('kernel32',use_last_error=True)
    class ThreadEntry(c.Structure):
        _fields_=[('size',w.DWORD),('usage',w.DWORD),('tid',w.DWORD),('pid',w.DWORD),('basePriority',w.LONG),('deltaPriority',w.LONG),('flags',w.DWORD)]
    class ModuleEntry(c.Structure):
        _fields_=[('size',w.DWORD),('moduleId',w.DWORD),('pid',w.DWORD),('globalUsage',w.DWORD),('processUsage',w.DWORD),('base',c.c_void_p),('bytes',w.DWORD),('module',w.HMODULE),('name',w.WCHAR*256),('path',w.WCHAR*260)]
    kernel.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD]; kernel.CreateToolhelp32Snapshot.restype=w.HANDLE
    kernel.CloseHandle.argtypes=[w.HANDLE]
    kernel.Module32FirstW.argtypes=[w.HANDLE,c.POINTER(ModuleEntry)]; kernel.Module32NextW.argtypes=kernel.Module32FirstW.argtypes
    kernel.Thread32First.argtypes=[w.HANDLE,c.POINTER(ThreadEntry)]; kernel.Thread32Next.argtypes=kernel.Thread32First.argtypes
    kernel.OpenThread.argtypes=[w.DWORD,w.BOOL,w.DWORD]; kernel.OpenThread.restype=w.HANDLE
    kernel.SuspendThread.argtypes=[w.HANDLE]; kernel.SuspendThread.restype=w.DWORD
    kernel.ResumeThread.argtypes=[w.HANDLE]
    kernel.GetThreadContext.argtypes=[w.HANDLE,c.c_void_p]; kernel.GetThreadContext.restype=w.BOOL
    kernel.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]; kernel.ReadProcessMemory.restype=w.BOOL
    modules=[]; snapshot=kernel.CreateToolhelp32Snapshot(0x18,pid)
    module=ModuleEntry(); module.size=c.sizeof(module)
    more=kernel.Module32FirstW(snapshot,c.byref(module))
    while more:
        modules.append({'base':module.base,'size':module.bytes,'name':module.name})
        more=kernel.Module32NextW(snapshot,c.byref(module))
    kernel.CloseHandle(snapshot)
    def describe(address):
        for module in modules:
            if module['base']<=address<module['base']+module['size']:
                return module['name']+'+0x'+format(address-module['base'],'x')
        return hex(address)
    threads=[]; snapshot=kernel.CreateToolhelp32Snapshot(4,0)
    entry=ThreadEntry(); entry.size=c.sizeof(entry)
    more=kernel.Thread32First(snapshot,c.byref(entry))
    while more:
        if entry.pid==pid:
            thread=kernel.OpenThread(0xA,False,entry.tid)
            if thread:
                # AMD64 CONTEXT must have 16-byte alignment.
                storage=c.create_string_buffer(1248)
                aligned=(c.addressof(storage)+15)&~15
                c.c_uint32.from_address(aligned+48).value=0x100003
                suspended=kernel.SuspendThread(thread)!=0xffffffff
                try:
                    if suspended and kernel.GetThreadContext(thread,aligned):
                        rip=c.c_uint64.from_address(aligned+248).value
                        rsp=c.c_uint64.from_address(aligned+152).value
                        stack=c.create_string_buffer(2048); read=c.c_size_t()
                        stack_frames=[]
                        if kernel.ReadProcessMemory(process,rsp,stack,len(stack),c.byref(read)):
                            for offset in range(0,read.value-7,8):
                                address=int.from_bytes(stack.raw[offset:offset+8],'little')
                                description=describe(address)
                                if '+' in description: stack_frames.append(description)
                        threads.append({'tid':entry.tid,'instruction':describe(rip),'stackCandidates':stack_frames[:50]})
                finally:
                    if suspended: kernel.ResumeThread(thread)
                    kernel.CloseHandle(thread)
        more=kernel.Thread32Next(snapshot,c.byref(entry))
    kernel.CloseHandle(snapshot)
    destination.write_text(json.dumps({'pid':pid,'threads':threads},indent=2))
