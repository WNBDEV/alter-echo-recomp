/*
 * New Game+ -- an extra, not part of the original game.
 *
 * Once a save from the final area is on disk, the main menu has a New Game+
 * entry under Load Game; until then the menu is the original's. Nothing
 * else changes unless that entry is picked (new_game_plus = 0 in
 * settings.ini, RECOMP_NEW_GAME_PLUS=0, takes it away). It opens the
 * game's own Load Game screen; the save picked there starts a new campaign
 * with that save's money, and every upgrade that save owned comes back as
 * the story makes it available again. It starts at L00_Master, the
 * prologue's last area: the prologue before it (L00_A1-A5) is the tutorial,
 * and L00_Master is where Paavo strips Nevin's forms and sets Plastids on
 * him, the start of the story the campaign plays out.
 *
 * What the game keeps, read off default.xbe and checked in guest memory:
 *
 *   0x25014C   money (int)
 *   0x250150   sword type: 0, or 1-3 once upgrade 42, 43 or 44 is bought
 *   0x546F38   50 upgrade slots (int): 0 not available yet, 1 can be
 *              bought, 2 owned. Slots 45-48 are the four guns (45, the
 *              default, always owned); the gun form's next-gun button
 *              (sub_000D4A30) skips any not owned
 *   0x42C310   the guns; +0x84 is the one in hand (0-3)
 *
 * A save (game.dat) is a 284-byte record: +0x00 area index (0-50),
 * +0x04 the 50 slots, +0xCC money, +0xD0 health, +0xD4-DC volumes, +0xE0
 * sword type, +0xE4 the gun in hand, +0xEC/F0/F4 play time (seconds as a
 * float, minutes, hours), +0xFC the share of upgrades owned (for the save
 * list), then a time stamp and a signature. Loading one
 * copies it into the record at 0x25067C and applies it with sub_000189D0;
 * saving builds one with sub_00018820.
 *
 * The story decides when each form is available: on every area load
 * sub_001419F0 makes slots available by area index and zeroes the gun
 * form's slots (14-26) in L00_A1-A3 and L01, the stealth form's (27-35)
 * until L00_A4 and in L01-L02, and the scripts give forms mid-area with
 * sub_00141EB0 (gun) and sub_00142000 (stealth). After each of those, New
 * Game+ gives back what the picked save owned, in two groups:
 *
 *   - the forms' own upgrades (14-35) once the story has made them
 *     available again ("can be bought", 1): they come back with the form,
 *     which the story still gives when it does;
 *   - everything else at once, even where the story would only make it
 *     buyable later: Nevin's own upgrades (health, the time dilation
 *     meter), the sword (42-44, types 1-3) and the extra guns (46-48). The
 *     player paid for them. The sword type follows the best sword owned, as
 *     buying it sets it (sub_00140B60), and the gun in hand stays the one
 *     the save had, an owned one. (The first version kept the gun in hand
 *     but gave the gun back only when buyable: it could be fired until put
 *     away and then never picked again.)
 *
 * The menu: the main menu (menu 12 of the manager at [0x1E7BD0]) is a list
 * of entry codes, drawn by sub_000B53F0 and acted on by sub_000B5750. Code 4
 * is a cut entry ("arena", string 0x53B8): picking it only plays the menu
 * sound. New Game+ is that entry with its text replaced.
 *
 * Tiers: a campaign started from a save in the final area (L09_A9) of a New
 * Game +N campaign is +N+1, up to +9; from any other save it stays at that
 * save's tier (a normal save counts as 0, and the lowest tier is +1). The
 * tier shows on screen as the campaign starts and when one of its saves is
 * loaded (the Load Game list cannot tell them from normal saves), through the game's own screen
 * text (SA_ScreenText_*, strings 0x53B8 and 0x5C15, a space).
 *
 * Every hit on Nevin is harder by the tier's factor (TIER_DAMAGE): all damage
 * goes through one hit handler, sub_0009E1A0, which works out damage = scale
 * (its 7th argument) * the attack's damage (+4), times Nevin's armour
 * (0x2500E8: 1.0 less 0.125 per armour upgrade, slots 36-39) when he is the
 * target. New Game+ multiplies the scale before the handler runs, so the
 * game's own death and damage handling sees the final hit, but never pushes
 * a hit past half of Nevin's health (100) unless the original hit was
 * already bigger. Enemies keep their health. Measured: a Plastid hit is 5,
 * late-game shots 2 (rapid) and 10-12, before armour.
 *
 * Saves made in a New Game+ campaign are noted in new_game_plus.txt next to
 * settings.ini, with their tier, so loading one later carries on.
 */
#include <xbox/xboxrecomp.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_esp;
extern int guest_vmem_allocate(uint32_t *base, uint32_t *size,
                               uint32_t alloc_type, uint32_t protect,
                               uint32_t *status);

#define GBYTES(va) ((uint8_t *)((uintptr_t)(uint32_t)(va) + (uintptr_t)xbox_GetMemoryOffset()))
#define G32(va)  (*(uint32_t *)GBYTES(va))
#define G8(va)   (*GBYTES(va))
#define GF(va)   (*(float *)GBYTES(va))

#define MONEY       0x25014Cu
#define SLOTS       0x546F38u
#define NSLOTS      50
#define MENUS       0x1E7BD0u   /* menu manager: menu n at [+0x1718 + 4n] */
#define MENU_SWITCH 0x1E7B20u   /* this for sub_00017850 (go to menu n) */
#define MENU_INPUT  0x1E7BE4u   /* byte: the front end takes input */
#define PAD         0x1E94C4u   /* this for sub_0002DD00 (A pressed) */
#define STRINGS     0x46BDBCu   /* sorted {id, text} pairs ... */
#define NSTRINGS    0x46BDB8u   /* ... and how many */
#define MAIN_MENU   12
#define LOAD_GAME   13
#define CODE_NGP    4           /* the cut "arena" entry */
#define STR_ARENA   0x53B8u
#define TEXT_PAGE   0x756A0000u /* just above the modules' region */

#define START_AREA  5           /* L00_Master, in the table at 0x1CA2B0 */
#define REC_AREA    0x00
#define REC_SLOTS   0x04
#define REC_MONEY   0xCC
#define REC_SWORD   0xE0
#define REC_GUN     0xE4
#define REC_TIME    0xEC        /* 3 words: seconds (float), minutes, hours */
#define SWORD       0x250150u
#define SLOT_SWORD1 42          /* 42, 43, 44: sword types 1, 2, 3 */
#define SLOT_GUN0   45          /* 45-48: guns 0-3 */

#define FINAL_AREA  50          /* L09_A9: a save there finished the game */
#define MAX_TIER    9
#define PLAYER      0x2500A0u   /* Nevin's object */
#define ARMOUR      0x2500E8u   /* float: damage taken, 0.5-1.0 */
#define OBJ_MAXHP   0xBC
#define HEAL        5           /* attack type (+0x1C) that heals */
#define NOTES_FILE  "new_game_plus.txt"
#define SAVES_DIR   "game/UDATA/5451000f"   /* a folder per save (main.c's game dir) */
#define MAX_NOTES   64

static int s_enabled = -1;
static int s_picking;           /* Load Game was opened from New Game+ */
static int s_active;            /* this campaign is a New Game+ one */
static int s_tier;              /* ... and its tier, 1-9 */
static int s_banner;            /* show "New Game Plus N" at the next start: */
static int s_banner_armed;      /* ... the area has started; waits for play */
static uint32_t s_text;         /* guest page: L"New Game Plus" at +0, 1 = none */

/* How much harder each tier hits: about 22% more per tier, from as hard as
 * a first playthrough with full armour (+1) to heavy hits taking half the
 * health bar (+9). */
static const float TIER_DAMAGE[MAX_TIER + 1] = {
    1.0f, 2.0f, 2.5f, 3.0f, 3.5f, 4.5f, 5.5f, 6.5f, 8.0f, 10.0f
};
static unsigned char s_owned[NSLOTS];

static int enabled(void)
{
    if (s_enabled < 0) {
        const char *e = getenv("RECOMP_NEW_GAME_PLUS");
        s_enabled = !e || !*e || atoi(e) > 0;
        if (!s_enabled) {
            fprintf(stderr, "[NG+] New Game+ is off (settings.ini new_game_plus = 0)\n");
            fflush(stderr);
        }
    }
    return s_enabled;
}

/* Calls into translated code, with a dummy return address on the guest
 * stack: each pops it (and its arguments) itself. */
static void call0(void (*fn)(void))
{
    g_esp -= 4;
    G32(g_esp) = 0;
    fn();
}

static void call_this1(void (*fn)(void), uint32_t self, uint32_t arg)
{
    g_esp -= 8;
    G32(g_esp) = 0;
    G32(g_esp + 4) = arg;
    g_ecx = self;
    fn();
}

static uint32_t menu(int n)
{
    uint32_t mgr = G32(MENUS);
    return mgr ? G32(mgr + 0x1718 + 4 * n) : 0;
}

/* ---- notes: which saves belong to a New Game+ campaign ---------------- */

typedef struct {
    int area, money;
    char slots[NSLOTS + 1];     /* the save's slots, '0'..'2' */
    char owned[NSLOTS + 1];     /* what the campaign brings back, '0'/'1' */
    int tier;
} Note;

static Note s_notes[MAX_NOTES];
static int s_nnotes = -1;

static void notes_load(void)
{
    FILE *f;
    char line[256];

    if (s_nnotes >= 0)
        return;
    s_nnotes = 0;
    f = fopen(NOTES_FILE, "r");
    if (!f)
        return;
    while (s_nnotes < MAX_NOTES && fgets(line, sizeof line, f)) {
        Note *n = &s_notes[s_nnotes];
        int k;
        n->tier = 1;            /* notes from before tiers */
        if (line[0] != '#'
                && ((k = sscanf(line, "%d %d %50s %50s %d", &n->area, &n->money,
                                n->slots, n->owned, &n->tier)) == 4 || k == 5)
                && strlen(n->slots) == NSLOTS && strlen(n->owned) == NSLOTS
                && n->tier >= 1 && n->tier <= MAX_TIER)
            s_nnotes++;
    }
    fclose(f);
}

static void notes_save(void)
{
    FILE *f = fopen(NOTES_FILE, "w");
    int i;

    if (!f) {
        fprintf(stderr, "[NG+] cannot write %s\n", NOTES_FILE);
        return;
    }
    fprintf(f, "# New Game+ saves: area money slots owned tier. Written by "
               "the game; deleting this file\n# makes those saves plain "
               "ones again.\n");
    for (i = 0; i < s_nnotes; i++)
        fprintf(f, "%d %d %s %s %d\n", s_notes[i].area, s_notes[i].money,
                s_notes[i].slots, s_notes[i].owned, s_notes[i].tier);
    fclose(f);
}

static void slots_text(char *out, uint32_t from)
{
    int i;
    for (i = 0; i < NSLOTS; i++) {
        uint32_t v = G32(from + 4 * i);
        out[i] = (char)('0' + (v <= 2 ? v : 9));
    }
    out[NSLOTS] = 0;
}

static Note *note_find(int area, int money, const char *slots)
{
    int i;
    for (i = 0; i < s_nnotes; i++)
        if (s_notes[i].area == area && s_notes[i].money == money
                && !strcmp(s_notes[i].slots, slots))
            return &s_notes[i];
    return NULL;
}

/* A save record was just built from the game: note it if this is a New
 * Game+ campaign (the newest note goes last; the oldest drops off). */
static void note_record(uint32_t rec)
{
    Note n, *old;
    int i;

    if (!s_active)
        return;
    notes_load();
    n.area = (int)G32(rec + REC_AREA);
    n.money = (int)G32(MONEY);
    slots_text(n.slots, SLOTS);
    for (i = 0; i < NSLOTS; i++)
        n.owned[i] = s_owned[i] ? '1' : '0';
    n.owned[NSLOTS] = 0;
    n.tier = s_tier;
    if ((old = note_find(n.area, n.money, n.slots)) != NULL
            && !strcmp(old->owned, n.owned) && old->tier == n.tier)
        return;
    if (old) {
        *old = n;
    } else {
        if (s_nnotes == MAX_NOTES)
            memmove(&s_notes[0], &s_notes[1], sizeof(Note) * (MAX_NOTES - 1)), s_nnotes--;
        s_notes[s_nnotes++] = n;
    }
    notes_save();
}

/* ---- bringing upgrades back -------------------------------------------- */

/* The sword type the slots at `slots` own: the best of upgrades 42-44. */
static uint32_t sword_owned(uint32_t slots)
{
    uint32_t t;
    for (t = 3; t > 0; t--)
        if (G32(slots + 4 * (SLOT_SWORD1 + t - 1)) == 2)
            return t;
    return 0;
}

/* The gun and stealth forms' slots: they come back with the form. */
static int form_slot(int i)
{
    return i >= 14 && i <= 35;
}

/* Whether slot i is or will be owned at once: owned in the record at `slots`,
 * or one of Nevin's own the campaign brings back immediately. */
static int owned_now(uint32_t slots, int i)
{
    return G32(slots + 4 * i) == 2 || (s_owned[i] && !form_slot(i));
}

static void bring_back(const char *why)
{
    int i, n = 0;
    uint32_t sword, v;

    if (!s_active)
        return;
    for (i = 0; i < NSLOTS; i++)
        if (s_owned[i] && (v = G32(SLOTS + 4 * i)) != 2
                && (v == 1 || !form_slot(i))) {
            G32(SLOTS + 4 * i) = 2;
            n++;
        }
    if (n) {
        fprintf(stderr, "[NG+] %s: %d upgrade(s) back\n", why, n);
        fflush(stderr);
    }
    if ((sword = sword_owned(SLOTS)) > G32(SWORD)) {
        G32(SWORD) = sword;
        fprintf(stderr, "[NG+] sword type %u back\n", sword);
        fflush(stderr);
    }
}

/* sub_001419F0: on every area load, the area's upgrade availability. */
extern void sub_001419F0_gen(void);
void sub_001419F0(void)
{
    sub_001419F0_gen();
    bring_back("area loaded");
}

/* sub_00141EB0 / sub_00142000: a script gives the gun / stealth form. */
extern void sub_00141EB0_gen(void);
void sub_00141EB0(void)
{
    sub_00141EB0_gen();
    bring_back("gun form given");
}

extern void sub_00142000_gen(void);
void sub_00142000(void)
{
    sub_00142000_gen();
    bring_back("stealth form given");
}

/* ---- starting and loading campaigns ----------------------------------- */

/* sub_000B56B0: the main menu's Start Game. A plain new campaign. */
extern void sub_000B56B0_gen(void);
void sub_000B56B0(void)
{
    s_active = 0;
    sub_000B56B0_gen();
}

/* sub_000189D0: apply a loaded save (thiscall, this = the record). From
 * New Game+ the picked save becomes a fresh campaign at START_AREA: its
 * money, settings, sword and gun in hand stay; the clock starts at zero
 * and the upgrades as a new game's (sub_001418D0, what Start Game uses),
 * to come back on the first area load (see bring_back). Otherwise a save
 * noted as New Game+ carries on as one. */
extern void sub_000189D0_gen(void);
extern void sub_001418D0(void);
void sub_000189D0(void)
{
    uint32_t rec = g_ecx;
    int i, picked = s_picking;

    if (!enabled()) {
        sub_000189D0_gen();
        return;
    }
    s_picking = 0;
    if (picked) {
        int n = 0, from = 0, area = (int)G32(rec + REC_AREA);
        char slots[NSLOTS + 1];
        Note *note;
        notes_load();
        slots_text(slots, rec + REC_SLOTS);
        if ((note = note_find(area, (int)G32(rec + REC_MONEY), slots)) != NULL)
            from = note->tier;
        s_tier = area >= FINAL_AREA ? from + 1 : from;
        if (s_tier < 1)
            s_tier = 1;
        if (s_tier > MAX_TIER)
            s_tier = MAX_TIER;
        for (i = 0; i < NSLOTS; i++)
            n += s_owned[i] = G32(rec + REC_SLOTS + 4 * i) == 2;
        s_active = 1;
        s_banner = 1;
        fprintf(stderr, "[NG+] New Game Plus %d, from a save in area %d (tier "
                "%d): %d owned upgrade(s), money %d\n", s_tier, area, from, n,
                (int)G32(rec + REC_MONEY));
        fflush(stderr);
        G32(rec + REC_AREA) = START_AREA;
        G32(rec + REC_TIME) = G32(rec + REC_TIME + 4) = G32(rec + REC_TIME + 8) = 0;
    } else {
        char slots[NSLOTS + 1];
        Note *note;
        notes_load();
        slots_text(slots, rec + REC_SLOTS);
        note = note_find((int)G32(rec + REC_AREA), (int)G32(rec + REC_MONEY), slots);
        s_active = note != NULL;
        if (note) {
            uint32_t gun = G32(rec + REC_GUN);
            s_tier = note->tier;
            s_banner = 1;
            for (i = 0; i < NSLOTS; i++)
                s_owned[i] = note->owned[i] == '1';
            uint32_t t;
            /* The sword and gun in hand as owned now (a save from a version
             * that held them back says less). */
            for (t = 3; t > 0 && !owned_now(rec + REC_SLOTS, SLOT_SWORD1 + (int)t - 1); t--)
                ;
            G32(rec + REC_SWORD) = t;
            if (gun > 3 || !owned_now(rec + REC_SLOTS, SLOT_GUN0 + (int)gun))
                G32(rec + REC_GUN) = 0;
            fprintf(stderr, "[NG+] loaded a New Game Plus %d save (sword type "
                    "%u, gun %u)\n", s_tier, G32(rec + REC_SWORD), G32(rec + REC_GUN));
            fflush(stderr);
        }
    }
    g_ecx = rec;
    sub_000189D0_gen();
    if (picked)
        call0(sub_001418D0);
}

/* sub_00018820: build a save record from the game (thiscall, this = the
 * record, one argument: the area name). */
extern void sub_00018820_gen(void);
static void show_banner(void);
void sub_00018820(void)
{
    uint32_t rec = g_ecx;
    sub_00018820_gen();
    if (s_enabled > 0)
        note_record(rec);
    /* Built when an area starts (and when saving): from here the banner
     * waits for the first frame without a cutscene, see sub_000E6760. */
    if (s_banner && s_active)
        s_banner_armed = 1;
}

/* ---- tiers: harder hits and the banner --------------------------------- */

/* Calls into a translated cdecl function with up to four arguments. */
static void call_cdecl(void (*fn)(void), int n, uint32_t a, uint32_t b,
                       uint32_t c, uint32_t d)
{
    uint32_t args[4] = { a, b, c, d };
    int i;
    g_esp -= 4 * (uint32_t)(n + 1);
    G32(g_esp) = 0;
    for (i = 0; i < n; i++)
        G32(g_esp + 4 + 4 * (uint32_t)i) = args[i];
    fn();                       /* pops the return address */
    g_esp += 4 * (uint32_t)n;   /* the caller pops the arguments */
}

static uint32_t fbits(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

extern void sub_00164370(void);  /* SA_ScreenText_CreateInstance(int *) */
extern void sub_00164390(void);  /* SA_ScreenText_SetScreenPos(h, x, y) */
extern void sub_001643B0(void);  /* SA_ScreenText_ClearString(h) */
extern void sub_001643C0(void);  /* SA_ScreenText_AppendText(h, string) */
extern void sub_001643E0(void);  /* SA_ScreenText_AppendInteger(h, n) */
extern void sub_00164420(void);  /* SA_ScreenText_InitTimers(h, in, out, hold) */
extern void sub_00164450(void);  /* SA_ScreenText_KillInstance(h) */

#define STR_SPACE   0x5C15u

/* "New Game Plus N" in the middle of the screen for a few seconds, the way
 * the tutorials show their captions. */
static void show_banner(void)
{
    static uint32_t handle;
    uint32_t h;

    if (s_text < 2)             /* no guest page: the label was never set */
        return;
    if (handle)
        call_cdecl(sub_00164450, 1, handle, 0, 0, 0);
    G32(s_text + 0x100) = 0xFFFFFFFFu;
    call_cdecl(sub_00164370, 1, s_text + 0x100, 0, 0, 0);
    h = G32(s_text + 0x100);
    if (h == 0xFFFFFFFFu) {
        fprintf(stderr, "[NG+] no screen text for the banner\n");
        return;
    }
    handle = h;
    call_cdecl(sub_001643B0, 1, h, 0, 0, 0);
    call_cdecl(sub_001643C0, 2, h, STR_ARENA, 0, 0);
    call_cdecl(sub_001643C0, 2, h, STR_SPACE, 0, 0);
    call_cdecl(sub_001643E0, 2, h, (uint32_t)s_tier, 0, 0);
    call_cdecl(sub_00164420, 4, h, fbits(0.5f), fbits(0.5f), fbits(4.0f));
    call_cdecl(sub_00164390, 3, h, fbits(0.5f), fbits(0.3f), 0);
    fprintf(stderr, "[NG+] banner: New Game Plus %d\n", s_tier);
    fflush(stderr);
}

/* sub_000E6760: the screen text's update, every frame (thiscall, this =
 * 0x46BDD8). It hides all screen text while a cutscene runs (the object at
 * 0x1E4A7C is non-zero then), so a banner made as the area starts would
 * time out unseen behind the arrival cutscene: it goes up on the first frame
 * of play instead. */
#define CUTSCENE    0x1E4A7Cu
extern void sub_000E6760_gen(void);
void sub_000E6760(void)
{
    sub_000E6760_gen();
    if (s_banner_armed && s_active && G32(CUTSCENE) == 0) {
        s_banner = s_banner_armed = 0;
        show_banner();
    }
}

/* sub_0009E1A0: the hit handler (cdecl): +4 the object hit, +8 the attack
 * (+4 its damage, +0x1C its type), +0x1C a float the damage is scaled by. */
extern void sub_0009E1A0_gen(void);
void sub_0009E1A0(void)
{
    uint32_t sp = g_esp, target, attack;

    if (s_active && s_tier >= 1 && (target = G32(sp + 4)) != 0
            && target == G32(PLAYER) && (attack = G32(sp + 8)) != 0
            && G32(attack + 0x1C) != HEAL) {
        float scale = GF(sp + 0x1C), base = GF(attack + 4);
        float maxhp = GF(target + OBJ_MAXHP), cap, est, g;
        cap = 0.5f * (maxhp > 0.0f ? maxhp : 100.0f);
        est = scale * base * GF(ARMOUR);
        g = TIER_DAMAGE[s_tier];
        if (est <= 0.0f || est >= cap)
            g = 1.0f;           /* nothing to scale, or already past the cap */
        else if (est * g > cap)
            g = cap / est;
        if (g != 1.0f) {
            GF(sp + 0x1C) = scale * g;
            if (getenv("RECOMP_NGP_TRACE")) {
                fprintf(stderr, "[NG+] hit %.2f x%.2f -> %.2f\n", est, g, est * g);
                fflush(stderr);
            }
        }
    }
    sub_0009E1A0_gen();
}

/* ---- the main menu entry ---------------------------------------------- */


/* Point the cut entry's text (string 0x53B8) at "New Game Plus". */
static void set_text(void)
{
    static const char label[] = "New Game Plus";   /* the menu font has no "+" */
    uint32_t base = G32(STRINGS), n = G32(NSTRINGS), lo = 0, hi;

    if (!s_text) {
        uint32_t b = TEXT_PAGE, size = 0x1000, status = 0;
        size_t i;
        if (!guest_vmem_allocate(&b, &size, 0x3000, 0x04, &status) || status) {
            fprintf(stderr, "[NG+] no guest memory for the menu text "
                    "(status %08X)\n", status);
            fflush(stderr);
            s_text = 1;     /* do not try again */
            return;
        }
        for (i = 0; i <= sizeof label - 1; i++) {
            GBYTES(b)[2 * i] = (uint8_t)label[i];
            GBYTES(b)[2 * i + 1] = 0;
        }
        s_text = b;
    }
    if (s_text == 1 || !base || !n)
        return;
    hi = n;
    while (lo < hi) {               /* the game's own lookup is a bsearch */
        uint32_t mid = (lo + hi) / 2, id = G32(base + 8 * mid);
        if (id == STR_ARENA) {
            G32(base + 8 * mid + 4) = s_text;
            return;
        }
        if (id < STR_ARENA)
            lo = mid + 1;
        else
            hi = mid;
    }
    {
        static int told;
        if (!told++) {
            fprintf(stderr, "[NG+] string %04X not in the string table\n", STR_ARENA);
            fflush(stderr);
        }
    }
}

/* Put the entry under Load Game. The list: [+0] cursor, [+4] count,
 * [+8] entry codes (room for 14). */
static void add_entry(void)
{
    uint32_t m = menu(MAIN_MENU), count, i, at;

    if (!m)
        return;
    count = G32(m + 4);
    if (count == 0 || count >= 14)
        return;
    for (i = 0; i < count; i++)
        if (G32(m + 8 + 4 * i) == CODE_NGP)
            return;
    for (at = 0; at < count; at++)
        if (G32(m + 8 + 4 * at) == 1)       /* Load Game */
            break;
    at = at < count ? at + 1 : count;
    for (i = count; i > at; i--)
        G32(m + 8 + 4 * i) = G32(m + 8 + 4 * (i - 1));
    G32(m + 8 + 4 * at) = CODE_NGP;
    G32(m + 4) = count + 1;
    fprintf(stderr, "[NG+] New Game+ added to the main menu\n");
    fflush(stderr);
}

/* Take the entry out again (the finished save is gone). */
static void remove_entry(void)
{
    uint32_t m = menu(MAIN_MENU), count, i;

    if (!m)
        return;
    count = G32(m + 4);
    for (i = 0; i < count && G32(m + 8 + 4 * i) != CODE_NGP; i++)
        ;
    if (i == count)
        return;
    for (; i + 1 < count; i++)
        G32(m + 8 + 4 * i) = G32(m + 8 + 4 * (i + 1));
    G32(m + 4) = --count;
    if (count && G32(m) >= count)
        G32(m) = count - 1;
}

/* The entry shows once the game has been finished: a save from the final
 * area is on disk. Each save is a folder of SAVES_DIR, its game.dat starting
 * with the area index. */
static int finished_save_exists(void)
{
    DIR *d = opendir(SAVES_DIR);
    struct dirent *e;
    int found = 0;

    if (!d)
        return 0;
    while (!found && (e = readdir(d)) != NULL) {
        char path[512];
        uint32_t area;
        FILE *f;
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof path, "%s/%s/game.dat", SAVES_DIR, e->d_name);
        if ((f = fopen(path, "rb")) != NULL) {
            found = fread(&area, 4, 1, f) == 1 && area == FINAL_AREA;
            fclose(f);
        }
    }
    closedir(d);
    return found;
}

/* sub_000B5CA0: the main menu is entered. */
extern void sub_000B5CA0_gen(void);
void sub_000B5CA0(void)
{
    sub_000B5CA0_gen();
    if (!enabled())
        return;
    s_picking = 0;
    set_text();
    if (finished_save_exists()) {
        add_entry();
    } else {
        static int told;
        remove_entry();
        if (!told++) {
            fprintf(stderr, "[NG+] no save from the final area yet: no New Game+ entry\n");
            fflush(stderr);
        }
    }
}

/* sub_000B5750: the main menu's input (menu states 1, 3, 4 and 5; 5 has its
 * own handling). A on New Game+ plays the menu sound (the cut entry's own
 * handling) and opens Load Game to pick the save. */
extern void sub_000B5750_gen(void);
extern void sub_0002DD00(void);
extern void sub_00017850(void);
void sub_000B5750(void)
{
    uint32_t m;
    int fire = 0;

    if (enabled() && G8(MENU_INPUT) == 1 && (m = menu(MAIN_MENU)) != 0
            && G32(m + 0x48) != 5 && G32(m) < G32(m + 4)
            && G32(m + 8 + 4 * G32(m)) == CODE_NGP) {
        g_ecx = G32(PAD);
        call0(sub_0002DD00);
        fire = (g_eax & 0xFF) != 0;
    }
    sub_000B5750_gen();
    if (fire) {
        s_picking = 1;
        call_this1(sub_00017850, MENU_SWITCH, LOAD_GAME);
        fprintf(stderr, "[NG+] pick the save to start New Game+ from\n");
        fflush(stderr);
    }
}
