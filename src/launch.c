/*
 * Starting the game without a launcher script -- double-clicking the .exe on
 * Windows, or run.bat. run.sh and run-linux.sh do four jobs before the game
 * starts, and set RECOMP_LAUNCHER to say so. Started any other way, this
 * does them instead, first thing in WinMain:
 *
 *   1. find the project folder -- the .exe lives in build-win/, the game
 *      files, settings.ini and logs/ one folder up -- and work from there;
 *   2. switch on what this game needs and read settings.ini (made from
 *      settings.example.ini the first time) into the RECOMP_* variables the
 *      runtime reads, never overriding one that is already set;
 *   3. send the log to logs/run-<date>-<time>.txt, opened with the system it
 *      ran on, keeping the newest ten.
 *
 * The setting names and defaults are run.sh's: change both together.
 */
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef AE_BUILD_DESCRIBE
#define AE_BUILD_DESCRIBE "unknown"
#endif
#ifndef AE_TOOLKIT_DESCRIBE
#define AE_TOOLKIT_DESCRIBE "unknown"
#endif

#ifdef _WIN32
#define LOG_PREFIX "run"
#define ENVIRON _environ                /* stdlib.h's */
#else
#define LOG_PREFIX "native"
extern char **environ;
#define ENVIRON environ
#endif

/* Set unless already set to something, as run.sh's ${VAR:=value}. */
static void env_default(const char *name, const char *value)
{
    const char *v = getenv(name);
    if (v && *v)
        return;
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static int exists(const char *path)
{
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

/* ---- 1. the project folder ------------------------------------------- */

static void go_to_project_folder(void)
{
#ifdef _WIN32
    wchar_t dir[MAX_PATH * 4];
    DWORD n;
    int up;

    if (exists("settings.example.ini"))
        return;
    n = GetModuleFileNameW(NULL, dir, (DWORD)(sizeof dir / sizeof dir[0]));
    if (!n || n >= sizeof dir / sizeof dir[0])
        return;
    for (up = 0; up < 2; up++) {            /* the .exe's folder, then its parent */
        wchar_t *cut = wcsrchr(dir, L'\\');
        if (!cut)
            return;
        *cut = 0;
        if (_wchdir(dir) == 0 && exists("settings.example.ini"))
            return;
    }
#else
    char dir[4096];
    ssize_t n;
    int up;

    if (exists("settings.example.ini"))
        return;
    n = readlink("/proc/self/exe", dir, sizeof dir - 1);
    if (n <= 0)
        return;
    dir[n] = 0;
    for (up = 0; up < 2; up++) {
        char *cut = strrchr(dir, '/');
        if (!cut)
            return;
        *cut = 0;
        if (chdir(dir) == 0 && exists("settings.example.ini"))
            return;
    }
#endif
}

/* ---- 2. defaults and settings.ini -------------------------------------- */

/* run.sh's list: without these the game runs but draws nothing and opens
 * no window (what each is for is in run.sh). */
static const char *const s_needed[] = {
    "RECOMP_EXT_VMA", "RECOMP_AC97_READY", "RECOMP_USB", "RECOMP_KEYBOARD",
    "RECOMP_IRQL_LOCK", "RECOMP_PB_EXEC", "RECOMP_TITLE_KEVENTS", "RECOMP_VBLANK",
    "RECOMP_FB_WINDOW", "RECOMP_GPU", "RECOMP_HEAP_VFREE", "RECOMP_CONTIG_FREE",
};

static const struct { const char *key, *var; } s_settings[] = {
    { "resolution_scale",      "RECOMP_SCALE" },
    { "fullscreen",            "RECOMP_FULLSCREEN" },
    { "rumble",                "RECOMP_RUMBLE" },
    { "reverb",                NULL },   /* retired: the game sends nothing to the reverb */
    { "direct_render_targets", "RECOMP_RT_DIRECT" },
    { "texture_filtering",     "RECOMP_TEXFILTER" },
    { "sharpen",               "RECOMP_SHARPEN" },
    { "widescreen",            "RECOMP_WIDESCREEN" },
    { "input",                 "RECOMP_INPUT" },
    { "show_fps",              "RECOMP_SHOW_FPS" },
    { "mouse_sensitivity",     "RECOMP_MOUSE_SENS" },
    { "mouse_invert_y",        "RECOMP_MOUSE_INVERT" },
    { "camera_flip_x",         "RECOMP_CAMERA_FLIP_X" },
    { "camera_flip_y",         "RECOMP_CAMERA_FLIP_Y" },
    { "mouse_deadzone",        "RECOMP_MOUSE_DEADZONE" },
    { "new_game_plus",         "RECOMP_NEW_GAME_PLUS" },
};

static char s_warnings[1024];

static void copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out;
    char buf[4096];
    size_t n;

    if (!in)
        return;
    out = fopen(to, "wb");
    if (out) {
        while ((n = fread(buf, 1, sizeof buf, in)) > 0)
            fwrite(buf, 1, n, out);
        fclose(out);
    }
    fclose(in);
}

/* Everything up to '#', with every space and tab removed -- run.sh strips
 * them all, so "WheelUp, WheelDown" binds the same as "WheelUp,WheelDown". */
static void squeeze(char *s)
{
    char *w = s;
    for (; *s && *s != '#'; s++)
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
            *w++ = *s;
    *w = 0;
}

static void read_settings(void)
{
    char line[512], var[96];
    FILE *f;

    if (!exists("settings.ini") && exists("settings.example.ini"))
        copy_file("settings.example.ini", "settings.ini");
    f = fopen("settings.ini", "r");
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *key = line, *val = strchr(line, '=');
        size_t i;
        int known = 0;

        if (!val)
            continue;
        *val++ = 0;
        squeeze(key);
        squeeze(val);
        if (!*key || !*val)
            continue;
        if (!strncmp(key, "bind_", 5)) {
            size_t j = 0;
            j += (size_t)snprintf(var, sizeof var, "RECOMP_BIND_");
            for (i = 5; key[i] && j + 1 < sizeof var; i++)
                var[j++] = (char)toupper((unsigned char)key[i]);
            var[j] = 0;
            env_default(var, val);
            continue;
        }
        for (i = 0; i < sizeof s_settings / sizeof s_settings[0]; i++)
            if (!strcmp(key, s_settings[i].key)) {
                if (s_settings[i].var)
                    env_default(s_settings[i].var, val);
                known = 1;
                break;
            }
        if (!known) {
            size_t used = strlen(s_warnings);
            snprintf(s_warnings + used, sizeof s_warnings - used,
                     "settings.ini: unknown setting '%s' (ignored)\n", key);
        }
    }
    fclose(f);
}

static void apply_settings(void)
{
    size_t i;

    for (i = 0; i < sizeof s_needed / sizeof s_needed[0]; i++)
        env_default(s_needed[i], "1");
    read_settings();
    /* run.sh's defaults after the file: the render scale, the main thread's
     * stack, effects that sample their own frames read on the GPU, the
     * camera turning the way the stick goes, widescreen, and the software
     * rasteriser's threads (used only with RECOMP_GPU=0). */
    env_default("RECOMP_SCALE", "2");
    env_default("RECOMP_STACK_KB", "2048");
    env_default("RECOMP_RT_DIRECT", "1");
    env_default("RECOMP_CAMERA_FLIP_X", "1");
    env_default("RECOMP_CAMERA_FLIP_Y", "1");
    env_default("RECOMP_WIDESCREEN", "1");
    env_default("RECOMP_RASTER_THREADS", "4");
}

/* ---- 3. the log --------------------------------------------------------- */

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Keep the newest `keep` logs of ours. The names carry the date and time,
 * so sorted by name they are sorted by age. */
static void prune_logs(int keep)
{
    char *names[512];
    int n = 0, i;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("logs\\" LOG_PREFIX "-*.txt", &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do
            if (n < 512)
                names[n++] = _strdup(fd.cFileName);
        while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir("logs");
    struct dirent *e;
    size_t plen = strlen(LOG_PREFIX "-");
    if (d) {
        while ((e = readdir(d)) && n < 512) {
            size_t len = strlen(e->d_name);
            if (!strncmp(e->d_name, LOG_PREFIX "-", plen) && len > 4
                    && !strcmp(e->d_name + len - 4, ".txt"))
                names[n++] = strdup(e->d_name);
        }
        closedir(d);
    }
#endif
    qsort(names, (size_t)n, sizeof names[0], cmp_str);
    for (i = 0; i < n; i++) {
        if (i < n - keep) {
            char path[600];
            snprintf(path, sizeof path, "logs/%s", names[i]);
            remove(path);
        }
        free(names[i]);
    }
}

static void write_system(FILE *f)
{
#ifdef _WIN32
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    const char *(*wine_version)(void) = (const char *(*)(void))(void *)
        (ntdll ? GetProcAddress(ntdll, "wine_get_version") : NULL);
    void (*wine_host)(const char **, const char **) = (void (*)(const char **, const char **))(void *)
        (ntdll ? GetProcAddress(ntdll, "wine_get_host_version") : NULL);
    LONG (WINAPI *rtl_version)(OSVERSIONINFOW *) = (LONG (WINAPI *)(OSVERSIONINFOW *))(void *)
        (ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : NULL);
    OSVERSIONINFOW os;
    char cpu[128] = "unknown";
    DWORD cpu_len = sizeof cpu;
    SYSTEM_INFO si;
    MEMORYSTATUSEX mem;
    DISPLAY_DEVICEA dd;

    memset(&os, 0, sizeof os);
    os.dwOSVersionInfoSize = sizeof os;
    if (rtl_version)
        rtl_version(&os);
    fprintf(f, "os: Windows %lu.%lu (build %lu)", (unsigned long)os.dwMajorVersion,
            (unsigned long)os.dwMinorVersion, (unsigned long)os.dwBuildNumber);
    if (wine_version) {
        const char *sys = "?", *rel = "?";
        if (wine_host)
            wine_host(&sys, &rel);
        fprintf(f, "; Wine %s on %s %s", wine_version(), sys, rel);
    }
    fprintf(f, "\n");
    RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                 "ProcessorNameString", RRF_RT_REG_SZ, NULL, cpu, &cpu_len);
    GetSystemInfo(&si);
    mem.dwLength = sizeof mem;
    GlobalMemoryStatusEx(&mem);
    fprintf(f, "cpu: %s, %lu threads; ram %.1f GB\n", cpu,
            (unsigned long)si.dwNumberOfProcessors, (double)mem.ullTotalPhys / 1073741824.0);
    memset(&dd, 0, sizeof dd);
    dd.cb = sizeof dd;
    fprintf(f, "gpu: %s\n", EnumDisplayDevicesA(NULL, 0, &dd, 0) ? dd.DeviceString : "unknown");
#else
    struct utsname u;
    char line[256], cpu[160] = "unknown";
    FILE *c = fopen("/proc/cpuinfo", "r");
    if (c) {
        while (fgets(line, sizeof line, c))
            if (!strncmp(line, "model name", 10) && strchr(line, ':')) {
                snprintf(cpu, sizeof cpu, "%s", strchr(line, ':') + 2);
                cpu[strcspn(cpu, "\n")] = 0;
                break;
            }
        fclose(c);
    }
    if (uname(&u) == 0)
        fprintf(f, "os: %s %s\n", u.sysname, u.release);
    fprintf(f, "cpu: %s, %ld threads; ram %.1f GB\n", cpu, sysconf(_SC_NPROCESSORS_ONLN),
            (double)sysconf(_SC_PHYS_PAGES) * (double)sysconf(_SC_PAGESIZE) / 1073741824.0);
#endif
}

static void open_log(void)
{
    char path[64], when[32];
    char *vars[256];
    int n = 0, i;
    time_t t = time(NULL);
    struct tm tm;
    FILE *f;
    char **e;

#ifdef _WIN32
    localtime_s(&tm, &t);
    _mkdir("logs");
#else
    localtime_r(&t, &tm);
    mkdir("logs", 0755);
#endif
    prune_logs(9);                      /* nine old ones and this one */
    strftime(when, sizeof when, "%Y%m%d-%H%M%S", &tm);
    snprintf(path, sizeof path, "logs/" LOG_PREFIX "-%s.txt", when);
    f = fopen(path, "w");
    if (!f)
        return;
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(f, "== Alter Echo recomp, %s\n", when);
    fprintf(f, "build: %s; toolkit %s\n", AE_BUILD_DESCRIBE, AE_TOOLKIT_DESCRIBE);
    write_system(f);
    for (e = ENVIRON; e && *e && n < 256; e++)
        if (!strncmp(*e, "RECOMP_", 7))
            vars[n++] = *e;
    qsort(vars, (size_t)n, sizeof vars[0], cmp_str);
    fprintf(f, "settings:");
    for (i = 0; i < n; i++)
        fprintf(f, " %s", vars[i]);
    fprintf(f, "\n==\n");
    fclose(f);
    if (freopen(path, "a", stderr))
        fputs(s_warnings, stderr);
}

void launch_init(void)
{
    const char *by = getenv("RECOMP_LAUNCHER");

    go_to_project_folder();
    if (by && *by)
        return;                         /* run.sh or run-linux.sh did the rest */
    apply_settings();
    open_log();
}
