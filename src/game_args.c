/*
 * The game's own command line, from RECOMP_ARGS.
 *
 * Alter Echo kept its development command line: sub_000164F0 parses argv into
 * the settings block at 0x1E7B68. A bare argument names the level to start
 * (".clf" is stripped), and the -switches turn on development options:
 *
 *   -bypass       skip the front end: boot straight into the level
 *   -nomovies -allowskip -noai -noshake -nomusic -nobars -16x9 -startslew
 *   -allmorphs -nohighlod -noscriptwarning -noerrorconsole -nodms
 *   -nosoundpaging -oldscripts -useclf -useulf -lang:<code>
 *
 * The retail build calls it with argc = 1 (sub_00052E20), so none of this is
 * reachable. Two places read the level name: the boot path (sub_0001FDE0,
 * with -bypass) and the main menu's Start Game (sub_000B56B0), which start it
 * instead of L00_A1. So
 *
 *   RECOMP_ARGS="L05_A1 -bypass" ./run.sh     boots into L05_A1
 *   RECOMP_ARGS="L05_A1" ./run.sh             Start Game goes to L05_A1
 *
 * Level names are the area files in clf/ (L00_A1 .. L09_A9). Everything is
 * handed to the game's parser unchanged; this only builds the argv it never
 * got, on the guest stack below the caller's frame.
 */
#include <xbox/xboxrecomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern RECOMP_TLS uint32_t g_esp;

#define GBYTES(va) ((uint8_t *)((uintptr_t)(uint32_t)(va) + (uintptr_t)xbox_GetMemoryOffset()))
#define G32(va)  (*(uint32_t *)GBYTES(va))

#define MAX_ARGS  16
#define ARG_AREA  512   /* guest stack bytes for the argv array and strings */

/* sub_000164F0: thiscall, ecx = settings block, stack: argc, argv; ret 8. */
extern void sub_000164F0_gen(void);
void sub_000164F0(void)
{
    const char *env = getenv("RECOMP_ARGS");
    uint32_t sp = g_esp, ret, argv0, area, argv, str;
    uint32_t argc = 1;
    const char *p;

    if (!env || !*env) {
        sub_000164F0_gen();
        return;
    }
    ret = G32(sp);
    argv0 = G32(G32(sp + 8));

    /* Below the caller's frame, above the parser's: the parser's pushes start
     * at area - 12 and grow down, away from it. */
    area = (sp - ARG_AREA) & ~15u;
    argv = area;
    str = area + 4 * MAX_ARGS;
    G32(argv) = argv0;
    for (p = env; *p && argc < MAX_ARGS; ) {
        size_t n;
        while (*p == ' ' || *p == '\t')
            p++;
        n = strcspn(p, " \t");
        if (!n)
            break;
        if (str + n + 1 > area + ARG_AREA) {
            fprintf(stderr, "[ARGS] RECOMP_ARGS too long, rest ignored: %s\n", p);
            break;
        }
        memcpy(GBYTES(str), p, n);
        GBYTES(str)[n] = 0;
        G32(argv + 4 * argc++) = str;
        str += n + 1;
        p += n;
    }
    fprintf(stderr, "[ARGS] game command line (%u args): %s\n", argc - 1, env);
    fflush(stderr);

    g_esp = area - 12;
    G32(g_esp) = ret;
    G32(g_esp + 4) = argc;
    G32(g_esp + 8) = argv;
    sub_000164F0_gen();             /* pops ret, argc, argv: g_esp == area */
    g_esp = sp + 12;                /* as the original ret 8 leaves it */
}
