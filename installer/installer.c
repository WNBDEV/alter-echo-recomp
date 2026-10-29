/*
 * AlterEchoInstaller.exe -- builds Alter Echo on a Windows PC from the
 * player's own disc image.
 *
 * The program is this stub with a payload appended (installer/make_installer.py
 * puts them together): a portable Python, the project's and the toolkit's
 * sources, and bootstrap.py. It asks where to install and for the disc image,
 * unpacks the payload there, and runs bootstrap.py in a console window, which
 * downloads the build tools and builds the game. Then it adds a Start Menu
 * shortcut.
 *
 *   AlterEchoInstaller.exe                       asks with the usual dialogs
 *   AlterEchoInstaller.exe /dir <folder> /iso <disc image> [/noshortcut]
 *                                                no dialogs (for testing)
 *   /jobs N                                      compile N files at a time
 *                                                (default: the PC's threads,
 *                                                as many as memory holds)
 *
 * Payload layout, at the end of the file:
 *   per file: u32 path length, path (UTF-8, '/' separators), u64 size, bytes
 *   trailer:  u64 offset of the first file, "AEINST01"
 */
#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#define TITLE L"Alter Echo installer"
#define MAGIC "AEINST01"

static wchar_t g_inst[MAX_PATH], g_iso[MAX_PATH];

static void error_box(const wchar_t *msg)
{
    MessageBoxW(NULL, msg, TITLE, MB_ICONERROR | MB_OK);
}

/* ---- the dialogs --------------------------------------------------------- */

static int ask_folder(void)
{
    BROWSEINFOW bi;
    PIDLIST_ABSOLUTE id;
    wchar_t base[MAX_PATH];

    ZeroMemory(&bi, sizeof bi);
    bi.lpszTitle = L"Choose where to install Alter Echo. A folder named \"Alter Echo\" "
                   L"is made inside it. It needs about 8 GB.";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    id = SHBrowseForFolderW(&bi);
    if (!id)
        return 0;
    if (!SHGetPathFromIDListW(id, base)) {
        CoTaskMemFree(id);
        return 0;
    }
    CoTaskMemFree(id);
    _snwprintf(g_inst, MAX_PATH, L"%s%sAlter Echo", base,
               base[wcslen(base) - 1] == L'\\' ? L"" : L"\\");
    g_inst[MAX_PATH - 1] = 0;
    return 1;
}

static int ask_iso(void)
{
    OPENFILENAMEW of;

    ZeroMemory(&of, sizeof of);
    of.lStructSize = sizeof of;
    of.lpstrFilter = L"Xbox disc image (*.iso)\0*.iso\0All files\0*.*\0";
    of.lpstrFile = g_iso;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Your Alter Echo disc image (North American release)";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&of);
}

/* ---- the payload --------------------------------------------------------- */

static int make_dirs(wchar_t *path)
{
    wchar_t *p;
    for (p = path + 3; *p; p++)             /* past "C:\" */
        if (*p == L'\\') {
            *p = 0;
            CreateDirectoryW(path, NULL);
            *p = L'\\';
        }
    return 1;
}

/* Whether the file at path already holds the next size bytes of f. Either
 * way f is left where it was. An update over an earlier install then leaves
 * the files that did not change as they were, times included, so the build
 * compiles only what changed. */
static int same_file(const wchar_t *path, FILE *f, uint64_t size)
{
    static char a[1 << 16], b[1 << 16];
    long long at = _ftelli64(f);
    uint64_t left = size;
    int same = 0;
    FILE *o = _wfopen(path, L"rb");

    if (!o)
        return 0;
    if (_fseeki64(o, 0, SEEK_END) == 0 && (uint64_t)_ftelli64(o) == size) {
        _fseeki64(o, 0, SEEK_SET);
        same = 1;
        while (left && same) {
            size_t n = left < sizeof a ? (size_t)left : sizeof a;
            if (fread(a, 1, n, f) != n || fread(b, 1, n, o) != n || memcmp(a, b, n))
                same = 0;
            left -= n;
        }
    }
    fclose(o);
    _fseeki64(f, at, SEEK_SET);
    return same;
}

static int unpack(void)
{
    wchar_t self[MAX_PATH], out[MAX_PATH * 2];
    char magic[8], name[1024];
    uint64_t start, end, size;
    uint32_t len;
    FILE *f;
    static char buf[1 << 16];

    GetModuleFileNameW(NULL, self, MAX_PATH);
    f = _wfopen(self, L"rb");
    if (!f)
        return 0;
    _fseeki64(f, -16, SEEK_END);
    end = (uint64_t)_ftelli64(f);
    if (fread(&start, 8, 1, f) != 1 || fread(magic, 8, 1, f) != 1 || memcmp(magic, MAGIC, 8)) {
        fclose(f);
        return 0;
    }
    _fseeki64(f, (long long)start, SEEK_SET);
    while ((uint64_t)_ftelli64(f) < end) {
        wchar_t wname[1024];
        FILE *o;
        int i;

        if (fread(&len, 4, 1, f) != 1 || len >= sizeof name || fread(name, 1, len, f) != len
                || fread(&size, 8, 1, f) != 1)
            break;
        name[len] = 0;
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 1024);
        for (i = 0; wname[i]; i++)
            if (wname[i] == L'/')
                wname[i] = L'\\';
        _snwprintf(out, MAX_PATH * 2, L"%s\\%s", g_inst, wname);
        out[MAX_PATH * 2 - 1] = 0;
        make_dirs(out);
        if (same_file(out, f, size)) {
            _fseeki64(f, (long long)size, SEEK_CUR);
            continue;
        }
        o = _wfopen(out, L"wb");
        if (!o) {
            fclose(f);
            return 0;
        }
        while (size) {
            size_t n = size < sizeof buf ? (size_t)size : sizeof buf;
            if (fread(buf, 1, n, f) != n || fwrite(buf, 1, n, o) != n) {
                fclose(o);
                fclose(f);
                return 0;
            }
            size -= n;
        }
        fclose(o);
    }
    fclose(f);
    return 1;
}

/* ---- the build and the shortcut ----------------------------------------- */

static DWORD run_bootstrap(void)
{
    wchar_t cmd[MAX_PATH * 4];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;

    _snwprintf(cmd, MAX_PATH * 4, L"\"%s\\python\\python.exe\" -u \"%s\\bootstrap.py\" \"%s\" \"%s\"",
               g_inst, g_inst, g_inst, g_iso);
    cmd[MAX_PATH * 4 - 1] = 0;
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, g_inst, &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static void game_paths(wchar_t *exe, wchar_t *dir)
{
    _snwprintf(dir, MAX_PATH, L"%s\\alter-echo-recomp", g_inst);
    _snwprintf(exe, MAX_PATH, L"%s\\build-win\\alter_echo_recomp.exe", dir);
}

static void add_shortcut(void)
{
    wchar_t programs[MAX_PATH], lnk[MAX_PATH], exe[MAX_PATH], dir[MAX_PATH];
    IShellLinkW *sl;
    IPersistFile *pf;

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programs)))
        return;
    _snwprintf(lnk, MAX_PATH, L"%s\\Alter Echo.lnk", programs);
    game_paths(exe, dir);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&sl)))
        return;
    IShellLinkW_SetPath(sl, exe);
    IShellLinkW_SetWorkingDirectory(sl, dir);
    IShellLinkW_SetDescription(sl, L"Alter Echo (static recompilation)");
    if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
        IPersistFile_Save(pf, lnk, TRUE);
        IPersistFile_Release(pf);
    }
    IShellLinkW_Release(sl);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    int argc, i, quiet = 0, shortcut = 1, unpack_only = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t msg[MAX_PATH * 3];
    DWORD code;

    (void)inst; (void)prev; (void)cmdline; (void)show;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (i = 1; argv && i < argc; i++) {
        if (!_wcsicmp(argv[i], L"/dir") && i + 1 < argc) {
            wcsncpy(g_inst, argv[++i], MAX_PATH - 1);
            quiet = 1;
        } else if (!_wcsicmp(argv[i], L"/iso") && i + 1 < argc) {
            wcsncpy(g_iso, argv[++i], MAX_PATH - 1);
        } else if (!_wcsicmp(argv[i], L"/noshortcut")) {
            shortcut = 0;
        } else if (!_wcsicmp(argv[i], L"/jobs") && i + 1 < argc) {
            /* bootstrap.py and setup_game.py inherit it */
            SetEnvironmentVariableW(L"JOBS", argv[++i]);
        } else if (!_wcsicmp(argv[i], L"/unpackonly")) {
            unpack_only = 1;            /* for testing the installer: no build */
        }
    }

    if (!quiet) {
        if (MessageBoxW(NULL,
                L"This builds Alter Echo for your PC from your own disc image of the North "
                L"American Xbox release.\n\n"
                L"It downloads about 270 MB of free build tools (GCC, CMake, Ninja, capstone), "
                L"needs about 8 GB of disk space, and takes 15-30 minutes while it translates "
                L"and compiles the game.\n\nNo game files are included or downloaded.",
                TITLE, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
            return 0;
        if (!ask_folder() || !ask_iso())
            return 0;
    }
    if (!g_inst[0] || !g_iso[0]) {
        error_box(L"Give both the install folder and the disc image: /dir <folder> /iso <image>.");
        return 2;
    }
    {
        wchar_t probe[MAX_PATH * 2];
        _snwprintf(probe, MAX_PATH * 2, L"%s\\", g_inst);
        make_dirs(probe);
    }
    if (!unpack()) {
        _snwprintf(msg, MAX_PATH * 3, L"Could not unpack the installer into\n%s", g_inst);
        error_box(msg);
        return 1;
    }
    if (unpack_only)
        return 0;
    code = run_bootstrap();
    if (code != 0) {
        _snwprintf(msg, MAX_PATH * 3,
                   L"The build stopped. What happened is in\n%s\\install.log\n\n"
                   L"Running the installer again into the same folder carries on from "
                   L"what is already downloaded.", g_inst);
        error_box(msg);
        return (int)code;
    }
    if (shortcut)
        add_shortcut();
    if (!quiet) {
        _snwprintf(msg, MAX_PATH * 3,
                   L"Alter Echo is installed in\n%s\n\nStart it from the Start Menu "
                   L"(Alter Echo). Play now?", g_inst);
        if (MessageBoxW(NULL, msg, TITLE, MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            wchar_t exe[MAX_PATH], dir[MAX_PATH];
            game_paths(exe, dir);
            ShellExecuteW(NULL, L"open", exe, NULL, dir, SW_SHOWNORMAL);
        }
    }
    return 0;
}
