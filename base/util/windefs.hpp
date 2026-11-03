#pragma once

#include <Windows.h>

// NOTE: Hoku; The Windows SDK can suck my gock.
#undef NEAR
#undef FAR
#undef near
#undef far

struct PEB_LDR_DATA
{
    u32        Length;
    u8         Initialized;
    void      *SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
};

struct PEB
{
    u8            reserved_0[ 2 ];
    u8            BeingDebugged;
    u8            reserved_1[ 1 ];
    void         *reserved_2[ 2 ];
    PEB_LDR_DATA *Ldr;
};

struct UNICODE_STRING
{
    u16      Length;
    u16      MaximumLength;
    wchar_t *Buffer;
};

struct LDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    void          *DllBase;
    void          *EntryPoint;
    u32            SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
};
