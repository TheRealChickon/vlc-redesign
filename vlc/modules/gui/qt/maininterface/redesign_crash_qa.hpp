// SPDX-License-Identifier: GPL-2.0-or-later
// Crash evidence only for this build's explicitly enabled development tests.
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <QMetaProperty>
#include <QtQml/private/qqmlpropertydata_p.h>
inline LONG WINAPI redesignCrashEvidence(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    wchar_t directory[32768];
    const DWORD count = GetEnvironmentVariableW(L"VLC_REDESIGN_QA_DIR", directory, 32768);
    if (!count || count > 32700) return EXCEPTION_CONTINUE_SEARCH;
    wcscat_s(directory, L"\\crash-stack.txt");
    HANDLE output = CreateFileW(directory, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;
    char line[256]; DWORD written;
    CONTEXT context = *exception->ContextRecord;
    const int length = snprintf(line,sizeof(line),"Fault RIP=%llx RDX=%llx RCX=%llx address=%llx\r\n", context.Rip,context.Rdx,context.Rcx,exception->ExceptionRecord->ExceptionInformation[1]);
    WriteFile(output,line,length,&written,nullptr);
    // Identify the still-live QObject/property being read by Qt's wrapper.
    // This diagnostic is only installed for the isolated development tests.
    void *vtable = nullptr;
    SIZE_T read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(context.Rsi), &vtable, sizeof(vtable), &read)
        && read == sizeof(vtable)) {
        MEMORY_BASIC_INFORMATION table{};
        if (VirtualQuery(vtable, &table, sizeof(table)) && table.State == MEM_COMMIT
            && table.Type == MEM_IMAGE) {
            auto *object = reinterpret_cast<QObject *>(context.Rsi);
            QQmlPropertyData property{};
            if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(context.Rbx), &property, sizeof(property), &read)
                && read == sizeof(property)) {
                const auto *meta = object->metaObject();
                const QByteArray description = QByteArray(meta->className()) + " object=" + object->objectName().toUtf8()
                    + " property=" + meta->property(property.coreIndex()).name() + "\r\n";
                WriteFile(output, description.constData(), description.size(), &written, nullptr);
            }
        }
    }
    for (int i=0; i<32 && context.Rip; ++i) {
        MEMORY_BASIC_INFORMATION memory{};
        VirtualQuery(reinterpret_cast<void *>(context.Rip), &memory, sizeof(memory));
        const int size = snprintf(line,sizeof(line),"%02d base=%llx offset=%llx\r\n",i,reinterpret_cast<unsigned long long>(memory.AllocationBase),context.Rip-reinterpret_cast<unsigned long long>(memory.AllocationBase));
        WriteFile(output,line,size,&written,nullptr);
        DWORD64 base=0;
        auto function = RtlLookupFunctionEntry(context.Rip,&base,nullptr);
        if (!function) break;
        void *handlerData; DWORD64 frame;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,function,&context,&handlerData,&frame,nullptr);
    }
    CloseHandle(output);
    return EXCEPTION_CONTINUE_SEARCH;
}
inline void installRedesignCrashEvidence()
{
    if (!qEnvironmentVariableIsEmpty("VLC_REDESIGN_QA_DIR")) SetUnhandledExceptionFilter(redesignCrashEvidence);
}
#else
inline void installRedesignCrashEvidence() {}
#endif
