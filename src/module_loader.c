/*
 * Runtime-loaded modules at the addresses they were translated at.
 *
 * Alter Echo keeps its game logic -- object, level and trigger scripts -- in
 * PE DLLs inside the .orf/.clf archives and loads them with its own loader,
 * sub_0011D860. That reads the sections back to back into a heap buffer and
 * relocates them for wherever the heap put it. Code at a heap address was
 * never translated, so every call into it was skipped ("[ICALL] target ... is
 * not code") and, among much else, Nevin never initialised.
 *
 * tools/build_modules.py gives every module a fixed effective base above the
 * RAM mirrors (RVA r at base + r), translates its code there, and generates
 * module_layout.c with each module's relocation sites. This wraps the loader:
 * once it has loaded and relocated a module into the heap, the image is moved
 * to base + rawptr0 and every site adjusted by the difference, which is
 * exactly the image the translation was made from. The loader object is then
 * pointed at the new copy:
 *
 *   [obj+0x24]  image: the first section's bytes (RVA rawptr0 onward)
 *   [obj+0x28]  export directory, inside the image
 *   [obj+0x2C]  rawptr0, the first section's file offset
 *   [obj+0x34]  the heap block itself -- left alone, the game frees it
 *
 * Needs RECOMP_EXT_VMA=1: the module region is above the RAM mirrors, where
 * only the extended guest address space can commit memory at a fixed address.
 */
#include <xbox/xboxrecomp.h>
#include <stdio.h>
#include <string.h>
#include "module_layout.h"

extern RECOMP_TLS uint32_t g_eax, g_ecx;
extern int guest_vmem_allocate(uint32_t *base, uint32_t *size,
                               uint32_t alloc_type, uint32_t protect,
                               uint32_t *status);

#define GPTR(va) ((uint8_t *)((uintptr_t)(uint32_t)(va) + (uintptr_t)xbox_GetMemoryOffset()))
#define G32(va)  (*(uint32_t *)GPTR(va))

/* Commit the whole module region once, on the first load. */
static int region_ready(void)
{
    static int state;       /* 0 untried, 1 committed, -1 failed */
    if (!state) {
        uint32_t base = g_ae_module_region, size = g_ae_module_span, status = 0;
        if (guest_vmem_allocate(&base, &size, 0x3000 /* COMMIT|RESERVE */,
                                0x04 /* PAGE_READWRITE */, &status)
                && status == 0) {
            state = 1;
            /* Calls into the region are calls into translated code; without
             * this RECOMP_ICALL rejects them as wild pointers. */
            xbox_AddCodeRange(g_ae_module_region,
                              g_ae_module_region + g_ae_module_span);
        } else {
            state = -1;
            fprintf(stderr, "[MODULE] cannot commit 0x%08X-0x%08X (status "
                    "%08X): run with RECOMP_EXT_VMA=1. Modules stay in the "
                    "heap and calls into them will be skipped.\n",
                    g_ae_module_region, g_ae_module_region + g_ae_module_span,
                    status);
        }
        fflush(stderr);
    }
    return state > 0;
}

static const AeModule *find_module(const char *name)
{
    unsigned i;
    for (i = 0; i < g_ae_module_count; i++)
        if (!_stricmp(g_ae_modules[i].name, name))
            return &g_ae_modules[i];
    return NULL;
}

static void place_module(uint32_t obj)
{
    static unsigned shown;
    uint32_t image = G32(obj + 0x24), exp = G32(obj + 0x28);
    uint32_t raw0 = G32(obj + 0x2C), dst, adj, i, limit;
    const AeModule *m;
    char name[64];

    if (!image || !exp)
        return;
    {   /* The DLL's own name, from its export directory. */
        const char *p = (const char *)GPTR(image + G32(exp + 12) - raw0);
        for (i = 0; i + 1 < sizeof name && p[i]; i++)
            name[i] = p[i];
        name[i] = 0;
    }
    m = find_module(name);
    if (!m) {
        fprintf(stderr, "[MODULE] %s is not in the module layout -- left at "
                "0x%08X, untranslated (re-run tools/extract_modules.py and "
                "build_modules.py?)\n", name, image);
        fflush(stderr);
        return;
    }
    if (raw0 != m->rawptr0) {
        fprintf(stderr, "[MODULE] %s: loaded with rawptr0 0x%X, layout says "
                "0x%X -- left in the heap\n", name, raw0, m->rawptr0);
        fflush(stderr);
        return;
    }
    if (!region_ready())
        return;

    dst = m->base + m->rawptr0;
    adj = dst - image;              /* = base - (image - rawptr0) */
    memmove(GPTR(dst), GPTR(image), m->packed);
    /* Every site the loader relocated, by the same rule (image + rva -
     * rawptr0), gets the difference between the two placements. */
    limit = m->packed;
    for (i = 0; i < m->nsites; i++) {
        uint32_t off = m->sites[i];
        if (off + 4 <= limit)
            G32(dst + off) += adj;
    }
    G32(obj + 0x24) = dst;
    G32(obj + 0x28) = exp + adj;

    if (shown++ < 40) {
        fprintf(stderr, "[MODULE] %s -> 0x%08X (from heap 0x%08X, %u sites)\n",
                name, m->base, image - raw0, m->nsites);
        fflush(stderr);
    }
}

/* sub_0011D860: thiscall, ecx = loader object, one stack argument (the
 * stream), returns success in al. */
extern void sub_0011D860_gen(void);
void sub_0011D860(void)
{
    uint32_t obj = g_ecx;
    sub_0011D860_gen();
    if (g_eax & 0xFFu)
        place_module(obj);
}

/* The guest return address at the top of the guest stack -- for logging
 * from files that cannot see the thread-local register file. */
extern RECOMP_TLS uint32_t g_esp;
uint32_t ae_guest_return_address(void)
{
    return g_esp ? G32(g_esp) : 0;
}

extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
void ae_guest_regs(uint32_t *ebx, uint32_t *esi, uint32_t *edi)
{
    *ebx = g_ebx; *esi = g_esi; *edi = g_edi;
}

/* An import thunk slot (src/module_thunks.c): 5 bytes at the start of a
 * module's .text that its DLLFixupNames patched with `jmp <engine function>`.
 * The caller has pushed its return address and arguments exactly as for the
 * engine function, so following the jmp is just calling the target: it
 * consumes them. A slot that was never patched still holds int3 -- on the
 * console that is a breakpoint; here it pops the return address like an
 * unresolved stub and says so. */
typedef void (*ae_func_t)(void);
extern ae_func_t recomp_lookup(uint32_t xbox_va);
extern ae_func_t recomp_lookup_manual(uint32_t xbox_va);

void ae_dyn_thunk(uint32_t slot)
{
    const uint8_t *p = GPTR(slot);
    if (p[0] == 0xE9) {
        uint32_t target = slot + 5 + (uint32_t)*(const int32_t *)(p + 1);
        ae_func_t fn = recomp_lookup_manual(target);
        if (!fn)
            fn = recomp_lookup(target);
        if (fn) {
            fn();
            return;
        }
        {
            static unsigned n;
            if (n++ < 20) {
                fprintf(stderr, "[MODULE] thunk 0x%08X jumps to 0x%08X, which "
                        "is not translated\n", slot, target);
                fflush(stderr);
            }
        }
    } else {
        static unsigned n;
        if (n++ < 20) {
            fprintf(stderr, "[MODULE] thunk 0x%08X called but never patched "
                    "(%02X)\n", slot, p[0]);
            fflush(stderr);
        }
    }
    g_esp += 4;
    g_eax = 0;
}
