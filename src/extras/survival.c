/*
 * Survival mode -- an extra, not part of the original game.
 *
 * The main menu has a Survival entry. It starts a run: Nevin with every
 * upgrade, on no save, in one of Paavo's training arenas, against waves of
 * the arena's own enemies, each bigger than the last, until he dies. Then the
 * run is over and the game goes back to the main menu. Nothing is saved.
 *
 * The arenas are the "Master" areas that end each chapter: the training
 * sessions where Paavo sets enemies on Nevin from spawners ("matcens")
 * placed in the level. This first version has one floor, L00_Master and its
 * ten Plastid spawners.
 *
 * What the game keeps, read off default.xbe and the level scripts:
 *
 *   0x52F4C4   the spawners, 0x2DC bytes each, the handle at +4 (the index
 *              is handle & [0x541454]): +0x10 how many it has spawned since
 *              its limits were reset, +0x22C how many of those are alive,
 *              +0x270 how many it spawns before it stops, +0x280 flags (1 on)
 *   0x546F38   the 50 upgrade slots (see new_game_plus.c); the bytes at
 *              0x546F28 and 0x547000 turn the gun and stealth forms on
 *
 * Starting a run does what Start Game does (sub_000B56B0), with the arena
 * as the area to load: the same path the game's own level warp takes.
 *
 * The arena's level script would play the story: in L00_Master Paavo's two
 * conversations, then he strips Nevin's forms and switches the spawners on.
 * During a run the script gets only the area's start (event 0x16F: it
 * switches the spawners off and groups them) and its handle bookkeeping
 * (0x202). Survival does the rest through the engine's script functions: it
 * moves Nevin to where the story's fight starts and runs the waves.
 *
 * The menu entry is the cut "Secrets" entry (code 3, string 0x51F3), which
 * only played the menu sound, with its text replaced. The run's own text
 * ("Wave" and so on) is added to the game's string table under new IDs.
 */
#include <xbox/xboxrecomp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "survival.h"

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_esp;
extern int guest_vmem_allocate(uint32_t *base, uint32_t *size,
                               uint32_t alloc_type, uint32_t protect,
                               uint32_t *status);

#define GBYTES(va) ((uint8_t *)((uintptr_t)(uint32_t)(va) + (uintptr_t)xbox_GetMemoryOffset()))
#define G32(va)  (*(uint32_t *)GBYTES(va))
#define G8(va)   (*GBYTES(va))
#define GF(va)   (*(float *)GBYTES(va))

#define MENUS       0x1E7BD0u   /* menu manager: menu n at [+0x1718 + 4n] */
#define MENU_SWITCH 0x1E7B20u   /* this for sub_00017850 (go to menu n) */
#define MENU_INPUT  0x1E7BE4u   /* byte: the front end takes input */
#define PAD         0x1E94C4u   /* this for sub_0002DD00 (A pressed) */
#define STRINGS     0x46BDBCu   /* sorted {id, text} pairs ... */
#define NSTRINGS    0x46BDB8u   /* ... and how many */
#define MAIN_MENU   12
#define LOADING     0x1A        /* the menu that loads an area */
#define LEAVE_AREA  0x23        /* unloads it, then the main menu (pause menu's Quit) */
#define CODE_SURV   3           /* the cut "Secrets" entry */
#define CODE_NGP    4           /* New Game+ (new_game_plus.c) */
#define CODE_LOAD   1
#define STR_SECRETS 0x51F3u
#define STR_SPACE   0x5C15u

#define CAMPAIGN    0x2500A8u   /* this for the new-game reset and armour */
#define SLOTS       0x546F38u
#define NSLOTS      50
#define GUN_FORM    0x546F28u
#define STEALTH     0x547000u
#define SWORD       0x250150u
#define PLAYER      0x2500A0u   /* Nevin's object */
#define OBJ_HP      0xB8
#define CUTSCENE    0x1E4A7Cu   /* non-zero while a cutscene runs */

#define MATCENS     0x52F4C4u
#define MATCEN_MASK 0x541454u
#define MATCEN_SIZE 0x2DCu
#define M_HANDLE    0x04
#define M_SPAWNED   0x10
#define M_ALIVE     0x22C
#define M_LIMIT     0x270

#define EV_START    0x16F       /* level scripts: the area has started */
#define EV_REMAP    0x202       /* ... a handle they keep has changed */
#define EV_HANDLED  0x100       /* what they return for an event they took */

/* Guest memory for the run: text the game reads, and the string table with
 * the run's strings added. */
#define PAGE        0x756B0000u /* above New Game+'s page */
#define PAGE_SIZE   0x10000u
#define P_LABEL     0x000       /* L"Survival" */
#define P_TEXT      0x040       /* the run's strings, 0x40 bytes each */
#define P_HANDLE    0x200       /* screen text handle, as it is made */
#define P_NAMES     0x400       /* names passed to the engine, 0x20 each */
#define P_TABLE     0x1000      /* the string table */

#define FPS         60          /* the frame hook runs once a frame */

typedef struct {
    const char *area;           /* the level file */
    const char *spot;           /* where the story's fight starts */
    const char *spawners[10];
} Floor;

static const Floor FLOORS[] = {
    { "L00_Master", "ConvPaavoNevin",
      { "matcen_Fodder01", "matcen_Fodder02", "matcen_Fodder03",
        "matcen_Fodder04", "matcen_Fodder05", "matcen_Fodder06",
        "matcen_Fodder07", "matcen_Fodder08", "matcen_Fodder09",
        "matcen_Fodder10" } },
};
#define NFLOORS ((int)(sizeof FLOORS / sizeof FLOORS[0]))
#define MAX_SPAWNERS 10

enum { T_WAVE, T_CLEARED, T_REACHED, NTEXTS };
static const char *const TEXTS[NTEXTS] = { "Wave", "cleared", "You reached wave" };

enum { OFF, LOADING_AREA, ARRIVING, RESTING, FIGHTING, OVER };

static int s_state = OFF;
static int s_floor;
static int s_wave;
static int s_frames;            /* frames left in this state, or counted */
static uint32_t s_page;         /* 0 not yet, 1 none to be had */
static uint32_t s_shown = 0xFFFFFFFFu;  /* the screen text up, or none */
static uint32_t s_ids[NTEXTS];
static uint32_t s_matcen[MAX_SPAWNERS];
static int s_base[MAX_SPAWNERS];    /* each spawner's own limit */
static int s_nmatcen;

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[SURV] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

/* ---- calls into translated code ----------------------------------------
 * A dummy return address on the guest stack: each callee pops it itself
 * (and its arguments, unless cdecl). */

static uint32_t call_cdecl(void (*fn)(void), int n, uint32_t a, uint32_t b,
                           uint32_t c, uint32_t d)
{
    uint32_t args[4] = { a, b, c, d };
    int i;
    g_esp -= 4 * (uint32_t)(n + 1);
    G32(g_esp) = 0;
    for (i = 0; i < n; i++)
        G32(g_esp + 4 + 4 * (uint32_t)i) = args[i];
    fn();
    g_esp += 4 * (uint32_t)n;
    return g_eax;
}

static uint32_t call_this(void (*fn)(void), uint32_t self, int n, uint32_t arg)
{
    g_esp -= 4 * (uint32_t)(n + 1);
    G32(g_esp) = 0;
    if (n)
        G32(g_esp + 4) = arg;
    g_ecx = self;
    fn();
    return g_eax;
}

extern void sub_0003CE50(void);  /* new game: reset the campaign (this CAMPAIGN) */
extern void sub_00027300(void);  /* ... (this 0x1E8FE0), as Start Game does */
extern void sub_00026820(void);
extern void sub_0003C810(void);  /* armour from slots 36-39 (this CAMPAIGN) */
extern void sub_00017420(void);  /* menu n (this [MENUS]) */
extern void sub_0001D5A0(void);  /* the area the loading menu loads (this that menu) */
extern void sub_00017850(void);  /* go to menu n (this MENU_SWITCH) */
extern void sub_0002DD00(void);  /* A pressed (this [PAD]) */
extern void sub_00125010(void);  /* VSS_GetObjectHandle(name) */
extern void sub_00125040(void);  /* VSS_GetMatcenHandle(name) */
extern void sub_0015D5C0(void);  /* SQ_Player_GetPlayerObject() */
extern void sub_001265E0(void);  /* SA_Obj_InstantlyMoveObjectToObject(obj, to) */
extern void sub_00162670(void);  /* SA_Matcen_ActivateMatcen(on, h) */
extern void sub_001626B0(void);  /* SA_Matcen_EnableMatcen(on, h) */
extern void sub_001626F0(void);  /* SA_Matcen_ResetMatcenLimits(h) */
extern void sub_00164370(void);  /* SA_ScreenText_CreateInstance(int *) */
extern void sub_00164390(void);  /* SA_ScreenText_SetScreenPos(h, x, y) */
extern void sub_001643B0(void);  /* SA_ScreenText_ClearString(h) */
extern void sub_001643C0(void);  /* SA_ScreenText_AppendText(h, string) */
extern void sub_001643E0(void);  /* SA_ScreenText_AppendInteger(h, n) */
extern void sub_00164420(void);  /* SA_ScreenText_InitTimers(h, in, out, hold) */
extern void sub_00164450(void);  /* SA_ScreenText_KillInstance(h) */

static uint32_t fbits(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static uint32_t menu(int n)
{
    uint32_t mgr = G32(MENUS);
    return mgr ? G32(mgr + 0x1718 + 4 * n) : 0;
}

/* ---- guest memory and text ---------------------------------------------- */

static void put_wide(uint32_t at, const char *s)
{
    size_t i;
    for (i = 0; i <= strlen(s); i++) {
        GBYTES(at)[2 * i] = (uint8_t)s[i];
        GBYTES(at)[2 * i + 1] = 0;
    }
}

/* An ASCII name for the engine, in slot i of the names area. */
static uint32_t guest_name(int i, const char *s)
{
    uint32_t at = s_page + P_NAMES + 0x20u * (uint32_t)i;
    memcpy(GBYTES(at), s, strlen(s) + 1);
    return at;
}

static int page(void)
{
    if (!s_page) {
        uint32_t b = PAGE, size = PAGE_SIZE, status = 0;
        int i;
        if (!guest_vmem_allocate(&b, &size, 0x3000, 0x04, &status) || status) {
            b = 0, size = PAGE_SIZE, status = 0;
            if (!guest_vmem_allocate(&b, &size, 0x3000, 0x04, &status) || status) {
                say("no guest memory (status %08X)", status);
                s_page = 1;
                return 0;
            }
        }
        s_page = b;
        put_wide(s_page + P_LABEL, "Survival");
        for (i = 0; i < NTEXTS; i++)
            put_wide(s_page + P_TEXT + 0x40u * (uint32_t)i, TEXTS[i]);
    }
    return s_page > 1;
}

/* The string table's entry for id, or 0. */
static uint32_t string_entry(uint32_t id)
{
    uint32_t base = G32(STRINGS), lo = 0, hi = G32(NSTRINGS);
    while (base && lo < hi) {               /* the game's own lookup is a bsearch */
        uint32_t mid = (lo + hi) / 2, v = G32(base + 8 * mid);
        if (v == id)
            return base + 8 * mid;
        if (v < id)
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

/* The game's string table, copied into the page with the run's strings
 * added after the highest ID (so it stays sorted), and the menu entry's
 * text pointed at "Survival". Done again if the game ever replaces it. */
static int strings(void)
{
    uint32_t base, n, top, room = (PAGE_SIZE - P_TABLE) / 8;
    int i;

    if (!page())
        return 0;
    base = G32(STRINGS);
    n = G32(NSTRINGS);
    if (!base || !n)
        return 0;
    if (base != s_page + P_TABLE) {
        if (n + NTEXTS > room) {
            say("string table too big (%u)", n);
            return 0;
        }
        memmove(GBYTES(s_page + P_TABLE), GBYTES(base), 8 * n);
        top = G32(base + 8 * (n - 1));
        for (i = 0; i < NTEXTS; i++) {
            s_ids[i] = top + 1 + (uint32_t)i;
            G32(s_page + P_TABLE + 8 * (n + (uint32_t)i)) = s_ids[i];
            G32(s_page + P_TABLE + 8 * (n + (uint32_t)i) + 4) =
                s_page + P_TEXT + 0x40u * (uint32_t)i;
        }
        G32(STRINGS) = s_page + P_TABLE;
        G32(NSTRINGS) = n + NTEXTS;
    }
    {
        uint32_t e = string_entry(STR_SECRETS);
        if (e)
            G32(e + 4) = s_page + P_LABEL;
    }
    return 1;
}

/* Text in the middle of the screen for `hold` seconds: a string, then,
 * unless n < 0, a space and n, then another string unless it's -1. */
static void show(int text, int n, int after, float hold)
{
    uint32_t h, slot;
    if (!strings())
        return;
    slot = s_page + P_HANDLE;
    if (s_shown != 0xFFFFFFFFu)
        call_cdecl(sub_00164450, 1, s_shown, 0, 0, 0);
    G32(slot) = 0xFFFFFFFFu;
    call_cdecl(sub_00164370, 1, slot, 0, 0, 0);
    if ((s_shown = h = G32(slot)) == 0xFFFFFFFFu) {
        say("no screen text");
        return;
    }
    call_cdecl(sub_001643B0, 1, h, 0, 0, 0);
    call_cdecl(sub_001643C0, 2, h, s_ids[text], 0, 0);
    if (n >= 0) {
        call_cdecl(sub_001643C0, 2, h, STR_SPACE, 0, 0);
        call_cdecl(sub_001643E0, 2, h, (uint32_t)n, 0, 0);
    }
    if (after >= 0) {
        call_cdecl(sub_001643C0, 2, h, STR_SPACE, 0, 0);
        call_cdecl(sub_001643C0, 2, h, s_ids[after], 0, 0);
    }
    call_cdecl(sub_00164420, 4, h, fbits(0.3f), fbits(0.5f), fbits(hold));
    call_cdecl(sub_00164390, 3, h, fbits(0.5f), fbits(0.3f), 0);
}

/* ---- the menu entry ------------------------------------------------------ */

/* Put the entry after New Game+, or after Load Game. The list: [+0] cursor,
 * [+4] count, [+8] entry codes (room for 14). */
static void add_entry(void)
{
    uint32_t m = menu(MAIN_MENU), count, i, at, after = CODE_LOAD;

    if (!m)
        return;
    count = G32(m + 4);
    if (count == 0 || count >= 14)
        return;
    for (i = 0; i < count; i++) {
        if (G32(m + 8 + 4 * i) == CODE_SURV)
            return;
        if (G32(m + 8 + 4 * i) == CODE_NGP)
            after = CODE_NGP;
    }
    for (at = 0; at < count && G32(m + 8 + 4 * at) != after; at++)
        ;
    at = at < count ? at + 1 : count;
    for (i = count; i > at; i--)
        G32(m + 8 + 4 * i) = G32(m + 8 + 4 * (i - 1));
    G32(m + 8 + 4 * at) = CODE_SURV;
    G32(m + 4) = count + 1;
    say("Survival added to the main menu");
}

void survival_menu_enter(void)
{
    s_state = OFF;
    if (strings())
        add_entry();
}

/* A on the entry. The menu's own input then plays the menu sound (all the
 * cut entry ever did) and survival_start() follows. */
int survival_menu_pressed(void)
{
    uint32_t m;

    if (G8(MENU_INPUT) != 1 || (m = menu(MAIN_MENU)) == 0
            || G32(m + 0x48) == 5 || G32(m) >= G32(m + 4)
            || G32(m + 8 + 4 * G32(m)) != CODE_SURV)
        return 0;
    g_ecx = G32(PAD);
    g_esp -= 4;
    G32(g_esp) = 0;
    sub_0002DD00();
    return (g_eax & 0xFF) != 0;
}

/* ---- the run ------------------------------------------------------------- */

/* Every upgrade, both forms, the best sword. Again on each area load, which
 * sets the forms by area. */
static void give_everything(void)
{
    int i;
    for (i = 0; i < NSLOTS; i++)
        G32(SLOTS + 4 * i) = 2;
    G8(GUN_FORM) = 1;
    G8(STEALTH) = 1;
    G32(SWORD) = 3;
    call_this(sub_0003C810, CAMPAIGN, 0, 0);
}

void survival_start(void)
{
    uint32_t load;

    if (!page())
        return;
    s_floor = 0;
    s_wave = 0;
    s_shown = 0xFFFFFFFFu;      /* the area it was in goes */
    s_state = LOADING_AREA;
    call_this(sub_0003CE50, CAMPAIGN, 0, 0);
    call_this(sub_00027300, 0x1E8FE0u, 0, 0);
    call_this(sub_00026820, 0x1E8FE0u, 0, 0);
    give_everything();
    load = call_this(sub_00017420, G32(MENUS), 1, LOADING);
    call_this(sub_0001D5A0, load, 1, guest_name(0, FLOORS[s_floor].area));
    call_this(sub_00017850, MENU_SWITCH, 1, LOADING);
    say("run started: floor %d, %s", s_floor + 1, FLOORS[s_floor].area);
}

void survival_stop(void)
{
    s_state = OFF;
}

void survival_area_loaded(uint32_t name)
{
    (void)name;
    if (s_state == OFF)
        return;
    give_everything();
    if (s_state == LOADING_AREA) {
        s_state = ARRIVING;
        s_frames = 0;
    }
}

static uint32_t spawner(int i)
{
    uint32_t h = s_matcen[i], m = MATCENS + MATCEN_SIZE * (h & G32(MATCEN_MASK));
    return (h & G32(MATCEN_MASK)) < 100 && G32(m + M_HANDLE) == h ? m : 0;
}

/* The floor's spawners, and Nevin to where the story's fight starts. */
static void arrive(void)
{
    const Floor *f = &FLOORS[s_floor];
    uint32_t spot, nevin;
    int i;

    s_nmatcen = 0;
    for (i = 0; i < MAX_SPAWNERS && f->spawners[i]; i++) {
        uint32_t h = call_cdecl(sub_00125040, 1, guest_name(1, f->spawners[i]), 0, 0, 0), m;
        s_matcen[s_nmatcen] = h;
        if (h == 0 || h == 0xFFFFFFFFu || !(m = spawner(s_nmatcen))) {
            say("no spawner %s", f->spawners[i]);
            continue;
        }
        s_base[s_nmatcen] = (int)G32(m + M_LIMIT);
        say("%s: limit %d, spawned %d, alive %d, flags %X", f->spawners[i],
            (int)G32(m + M_LIMIT), (int)G32(m + M_SPAWNED), (int)G32(m + M_ALIVE),
            G32(m + 0x280));
        s_nmatcen++;
    }
    spot = call_cdecl(sub_00125010, 1, guest_name(1, f->spot), 0, 0, 0);
    nevin = call_cdecl(sub_0015D5C0, 0, 0, 0, 0, 0);
    if (spot && spot != 0xFFFFFFFFu && nevin != 0xFFFFFFFFu)
        call_cdecl(sub_001265E0, 2, nevin, spot, 0, 0);
    else
        say("no %s to start at", f->spot);
    say("floor %d: %d spawner(s)", s_floor + 1, s_nmatcen);
}

/* How many each spawner makes in wave w: its own number first, then half
 * as many again each wave. */
static int wave_limit(int i, int w)
{
    int base = s_base[i] > 0 ? s_base[i] : 3;
    int step = base / 2 > 0 ? base / 2 : 1;
    return base + (w - 1) * step;
}

static void start_wave(void)
{
    int i;
    s_wave++;
    for (i = 0; i < s_nmatcen; i++) {
        uint32_t m = spawner(i);
        if (!m)
            continue;
        call_cdecl(sub_001626F0, 1, s_matcen[i], 0, 0, 0);
        G32(m + M_LIMIT) = (uint32_t)wave_limit(i, s_wave);
        call_cdecl(sub_001626B0, 2, 1, s_matcen[i], 0, 0);
        /* The first time on is enough; after a wave the spawner stays
         * active (flag 4) and only starts again from off. */
        if (s_wave > 1) {
            call_cdecl(sub_00162670, 2, 0, s_matcen[i], 0, 0);
            call_cdecl(sub_00162670, 2, 1, s_matcen[i], 0, 0);
        }
    }
    show(T_WAVE, s_wave, -1, 2.5f);
    s_state = FIGHTING;
    s_frames = 0;
    say("wave %d", s_wave);
}

/* Every spawner has made its number and all of them are dead. */
static int wave_cleared(void)
{
    int i, spawned = 0, alive = 0, left = 0;
    for (i = 0; i < s_nmatcen; i++) {
        uint32_t m = spawner(i);
        if (!m)
            continue;
        spawned += (int)G32(m + M_SPAWNED);
        alive += (int)G32(m + M_ALIVE);
        if ((int)G32(m + M_SPAWNED) < (int)G32(m + M_LIMIT))
            left++;
    }
    if (++s_frames % (10 * FPS) == 0)
        say("wave %d: %d spawned, %d alive, %d spawner(s) not done",
            s_wave, spawned, alive, left);
    return spawned > 0 && alive == 0 && left == 0;
}

static void end_wave(void)
{
    int i;
    for (i = 0; i < s_nmatcen; i++)
        if (spawner(i))
            call_cdecl(sub_001626B0, 2, 0, s_matcen[i], 0, 0);
    show(T_WAVE, s_wave, T_CLEARED, 2.0f);
    say("wave %d cleared", s_wave);
    s_state = RESTING;
    s_frames = 4 * FPS;
}

static int nevin_dead(void)
{
    uint32_t obj = G32(PLAYER);
    return obj && GF(obj + OBJ_HP) <= 0.0f;
}

void survival_frame(void)
{
    switch (s_state) {
    case ARRIVING:
        /* the area's arrival: wait for play, then a second more */
        if (G32(CUTSCENE) != 0 || !G32(PLAYER))
            s_frames = 0;
        else if (++s_frames == FPS) {
            arrive();
            s_state = RESTING;
            s_frames = 2 * FPS;
        }
        break;
    case RESTING:
    case FIGHTING:
        if (nevin_dead()) {
            show(T_REACHED, s_wave, -1, 5.0f);
            say("run over at wave %d", s_wave);
            s_state = OVER;
            s_frames = 6 * FPS;
        } else if (s_state == RESTING) {
            if (--s_frames <= 0)
                start_wave();
        } else if (wave_cleared()) {
            end_wave();
        }
        break;
    case OVER:
        if (--s_frames <= 0) {
            s_state = OFF;
            call_this(sub_00017850, MENU_SWITCH, 1, LEAVE_AREA);
            say("back to the main menu");
        }
        break;
    default:
        break;
    }
}

/* ---- the arenas' level scripts ------------------------------------------
 * CallInstanceEvent (cdecl: +8 the instance, +0xC the event, +0x10 its
 * data) of each arena's level_*.dll. During a run on that floor only the
 * area's start and handle bookkeeping get through. All the training arenas
 * and both Paavo Echo arenas are wrapped, for the floors to come. */

static int script_event(int floor)
{
    uint32_t ev;
    if (s_state == OFF || floor != s_floor)
        return 1;
    ev = G32(g_esp + 0xC);
    if (ev == EV_START || ev == EV_REMAP)
        return 1;
    g_eax = EV_HANDLED;
    g_esp += 4;                 /* ret */
    return 0;
}

extern void sub_740A2230_gen(void);     /* level_L00_Master */
void sub_740A2230(void)
{
    if (script_event(0))
        sub_740A2230_gen();
}

extern void sub_740E2210_gen(void);     /* level_L01_Master */
void sub_740E2210(void)
{
    if (script_event(1))
        sub_740E2210_gen();
}

extern void sub_741721E0_gen(void);     /* level_L02_Master */
void sub_741721E0(void)
{
    if (script_event(2))
        sub_741721E0_gen();
}

extern void sub_741C21D0_gen(void);     /* level_L03_Master */
void sub_741C21D0(void)
{
    if (script_event(3))
        sub_741C21D0_gen();
}

extern void sub_742721C0_gen(void);     /* level_L04_Master */
void sub_742721C0(void)
{
    if (script_event(4))
        sub_742721C0_gen();
}

extern void sub_743820A0_gen(void);     /* level_L06_Master */
void sub_743820A0(void)
{
    if (script_event(5))
        sub_743820A0_gen();
}

extern void sub_744D21C0_gen(void);     /* level_L08_Master */
void sub_744D21C0(void)
{
    if (script_event(6))
        sub_744D21C0_gen();
}

extern void sub_742C2560_gen(void);     /* level_L05_A3 (Paavo Echo) */
void sub_742C2560(void)
{
    if (script_event(7))
        sub_742C2560_gen();
}

extern void sub_743C2480_gen(void);     /* level_L07_A3 (Paavo Echo) */
void sub_743C2480(void)
{
    if (script_event(8))
        sub_743C2480_gen();
}
