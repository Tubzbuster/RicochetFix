/* RicochetFix - import address table patching. */
#include "rf.h"

void RF_PatchPtr(void **slot, void *value)
{
    DWORD old;
    if (VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) {
        *slot = value;
        VirtualProtect(slot, sizeof(void *), old, &old);
    }
}

static IMAGE_IMPORT_DESCRIPTOR *FirstImport(HMODULE mod)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;

    if (!mod || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return NULL;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return NULL;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress || !dir->Size)
        return NULL;
    return (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
}

/* Find the IAT slot for dll!func in mod. Matches by import name, or by
 * address when the import has no name table. */
static void **FindSlot(HMODULE mod, const char *dll, const char *func)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_IMPORT_DESCRIPTOR *d = FirstImport(mod);
    HMODULE target = GetModuleHandleA(dll);
    FARPROC real = target ? GetProcAddress(target, func) : NULL;

    for (; d && d->Name; d++) {
        IMAGE_THUNK_DATA *iat, *oft;
        int i;
        if (lstrcmpiA((const char *)(base + d->Name), dll))
            continue;
        iat = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        oft = d->OriginalFirstThunk ? (IMAGE_THUNK_DATA *)(base + d->OriginalFirstThunk) : NULL;
        for (i = 0; iat[i].u1.Function; i++) {
            if (oft && !(oft[i].u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                IMAGE_IMPORT_BY_NAME *n = (IMAGE_IMPORT_BY_NAME *)(base + oft[i].u1.AddressOfData);
                if (!lstrcmpA((const char *)n->Name, func))
                    return (void **)&iat[i].u1.Function;
            } else if (real && (FARPROC)iat[i].u1.Function == real) {
                return (void **)&iat[i].u1.Function;
            }
        }
    }
    return NULL;
}

int RF_ModuleImports(HMODULE mod, const char *dll, const char *func)
{
    return FindSlot(mod, dll, func) != NULL;
}

int RF_PatchIAT(HMODULE mod, const char *dll, const char *func, void *hook, void **orig)
{
    void **slot = FindSlot(mod, dll, func);
    if (!slot)
        return 0;
    if (*slot == hook)
        return 1;
    if (orig && !*orig)
        *orig = *slot;
    RF_PatchPtr(slot, hook);
    return 1;
}
