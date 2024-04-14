#pragma once
#pragma warning(disable: 4996)
#pragma warning(disable: 4201)

#define ActiveProcessLinksOffset 0x448
#define UniqueProcessIdOffset 0x440
#define ProcessProtectionOffset 0x87a
#define ImageFileNameOffset 0x5a8

typedef struct _MODULE_INFO {
    ULONG_PTR BaseAddress;
    ULONG_PTR ImageSize;
    UCHAR FullPathName[256];
} MODULE_INFO, * PMODULE_INFO;

typedef struct _PS_PROTECTION {
    union {
        UCHAR Level;
        struct {
            UCHAR Type : 3;
            UCHAR Audit : 1;
            UCHAR Signer : 4;
        };
    };
} PS_PROTECTION, * PPS_PROTECTION;

typedef enum _PS_PROTECTED_TYPE {
    PsProtectedTypeNone = 0,
    PsProtectedTypeProtectedLight = 1,
    PsProtectedTypeProtected = 2
} PS_PROTECTED_TYPE, * PPS_PROTECTED_TYPE;

typedef enum _PS_PROTECTED_SIGNER {
    PsProtectedSignerNone = 0,
    PsProtectedSignerAuthenticode,
    PsProtectedSignerCodeGen,
    PsProtectedSignerAntimalware,
    PsProtectedSignerLsa,
    PsProtectedSignerWindows,
    PsProtectedSignerWinTcb,
    PsProtectedSignerWinSystem,
    PsProtectedSignerApp,
    PsProtectedSignerMax
} PS_PROTECTED_SIGNER, * PPS_PROTECTED_SIGNER;

const char* ProtectedTypeStrings[] = {
    "None",
    "Protected Light",
    "Protected"
};

const char* ProtectedSignerStrings[] = {
    "None",
    "Authenticode",
    "CodeGen",
    "Antimalware",
    "Lsa",
    "Windows",
    "WinTcb",
    "WinSystem",
    "App",
    "Max"
};