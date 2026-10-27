// DST Sync - dong bo save world Don't Starve Together qua repo Git.
// Win32 + C++17, khong can thu vien ngoai.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::wstring;

// ============================================================ tien ich chuoi
static wstring utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::string wToUtf8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static wstring acpToW(const char* s) {
    int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (n <= 1) return L"";
    wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, &w[0], n);
    return w;
}
static wstring trimW(const wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    size_t a = s.find_first_not_of(ws);
    if (a == wstring::npos) return L"";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}
static wstring replaceAll(wstring s, const wstring& from, const wstring& to) {
    if (from.empty()) return s;
    size_t p = 0;
    while ((p = s.find(from, p)) != wstring::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}
static wstring q(const wstring& s) { return L"\"" + s + L"\""; }
static wstring lower(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static wstring fmtTime(time_t t) {
    wchar_t b[64]; tm* lt = localtime(&t);
    if (!lt) return L"?";
    wcsftime(b, 64, L"%d/%m/%Y %H:%M", lt);
    return b;
}
static wstring stamp() {
    wchar_t b[64]; time_t t = time(nullptr); tm* lt = localtime(&t);
    wcsftime(b, 64, L"%Y%m%d-%H%M%S", lt);
    return b;
}
static wstring cleanPath(wstring p) {
    p = trimW(p);
    if (p.empty()) return p;
    p = fs::path(p).lexically_normal().wstring();
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}
static fs::path exeDir() {
    wchar_t b[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    return fs::path(b).parent_path();
}
static wstring getEnvW(const wchar_t* k) {
    wchar_t b[512]; DWORD n = GetEnvironmentVariableW(k, b, 512);
    return (n > 0 && n < 512) ? wstring(b, n) : L"";
}

// ============================================================ cau hinh
struct Cfg {
    wstring name, url, token, repoDir, cluster, saveRoot, backupDir, lastSynced;
    int keep = 5;
};
static Cfg g_cfg;
static fs::path cfgPath() { return exeDir() / L"dstsync.cfg"; }

static bool cfgComplete() {
    return !g_cfg.name.empty() && !g_cfg.url.empty() && !g_cfg.repoDir.empty() &&
           !g_cfg.cluster.empty() && !g_cfg.saveRoot.empty() && !g_cfg.backupDir.empty();
}
static void defaultCfg() {
    wchar_t docs[MAX_PATH] = {0};
    SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs);
    g_cfg.saveRoot = (fs::path(docs) / L"Klei" / L"DoNotStarveTogether").wstring();
    g_cfg.repoDir = (exeDir() / L"repo").wstring();
    g_cfg.backupDir = (exeDir() / L"backups").wstring();
    g_cfg.name = getEnvW(L"USERNAME");
    g_cfg.cluster = L"Cluster_1";
}
static void loadCfg() {
    std::ifstream f(cfgPath(), std::ios::binary);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line.erase(0, 3); // BOM
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        wstring v = utf8ToW(line.substr(eq + 1));
        if (k == "name") g_cfg.name = v;
        else if (k == "url") g_cfg.url = v;
        else if (k == "token") g_cfg.token = v;
        else if (k == "repo") g_cfg.repoDir = v;
        else if (k == "cluster") g_cfg.cluster = v;
        else if (k == "saveroot") g_cfg.saveRoot = v;
        else if (k == "backup") g_cfg.backupDir = v;
        else if (k == "keep") g_cfg.keep = std::max(1, (int)wcstol(v.c_str(), nullptr, 10));
        else if (k == "lastsynced") g_cfg.lastSynced = v;
    }
}
static void saveCfg() {
    std::ofstream f(cfgPath(), std::ios::binary | std::ios::trunc);
    auto w = [&](const char* k, const wstring& v) { f << k << "=" << wToUtf8(v) << "\n"; };
    w("name", g_cfg.name); w("url", g_cfg.url); w("token", g_cfg.token);
    w("repo", g_cfg.repoDir); w("cluster", g_cfg.cluster); w("saveroot", g_cfg.saveRoot);
    w("backup", g_cfg.backupDir); w("keep", std::to_wstring(g_cfg.keep));
    w("lastsynced", g_cfg.lastSynced);
}

// ============================================================ UI toan cuc
enum {
    WM_LOG = WM_APP + 1, WM_ASK, WM_DONE, WM_STATUS, WM_INIT_APP
};
enum {
    ID_PULL = 101, ID_PUSH, ID_STATUS, ID_SETTINGS, ID_BACKUPS, ID_GETGIT,
    ID_LOG = 120, ID_LABEL_STATUS, ID_LABEL_GIT
};
enum Op { OP_PULL, OP_PUSH, OP_STATUS, OP_GETGIT };

static HINSTANCE g_hInst;
static HWND g_main, g_log, g_lblStatus, g_lblGit, g_setup;
static HWND g_btns[6];
static HFONT g_font;
static std::atomic<bool> g_busy{false};
static wstring g_git;

struct AskData { wstring title, text; UINT flags; int result; };

static void logw(wstring s) {
    if (!g_cfg.token.empty()) s = replaceAll(s, g_cfg.token, L"***");
    s = replaceAll(s, L"\r\n", L"\n");
    s = replaceAll(s, L"\n", L"\r\n");
    s += L"\r\n";
    PostMessageW(g_main, WM_LOG, 0, (LPARAM) new wstring(std::move(s)));
}
static int ask(const wstring& title, const wstring& text, UINT flags) {
    AskData a{title, text, flags, 0};
    SendMessageW(g_main, WM_ASK, 0, (LPARAM)&a);
    return a.result;
}

// ============================================================ chay tien trinh
static int runProcess(const wstring& cmdline, const wstring& cwd, wstring* outAll, bool echo) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = nul;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmdline.begin(), cmdline.end());
    buf.push_back(0);
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return -1; }

    std::string pending; char chunk[4096]; DWORD n = 0;
    auto flushLine = [&](std::string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        wstring w = utf8ToW(line);
        if (outAll) { *outAll += w; *outAll += L"\n"; }
        if (echo) logw(L"    " + w);
    };
    while (ReadFile(rd, chunk, sizeof chunk, &n, nullptr) && n) {
        pending.append(chunk, n);
        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            flushLine(pending.substr(0, pos));
            pending.erase(0, pos + 1);
        }
    }
    if (!pending.empty()) flushLine(pending);
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)code;
}

static wstring findGit() {
    fs::path d = exeDir();
    std::vector<fs::path> c = {d / L"git" / L"cmd" / L"git.exe", d / L"git" / L"bin" / L"git.exe",
                               d / L"PortableGit" / L"cmd" / L"git.exe"};
    for (auto& p : c) if (fs::exists(p)) return p.wstring();
    wchar_t b[MAX_PATH];
    if (SearchPathW(nullptr, L"git.exe", nullptr, MAX_PATH, b, nullptr)) return b;
    std::vector<wstring> pf = {L"C:\\Program Files\\Git\\cmd\\git.exe", L"C:\\Program Files (x86)\\Git\\cmd\\git.exe"};
    for (auto& p : pf) if (fs::exists(p)) return p;
    return L"";
}

static int git(const wstring& args, wstring* out = nullptr, bool echo = true, bool inRepo = true) {
    wstring cmd = q(g_git) + L" -c core.quotepath=false -c core.autocrlf=false -c core.safecrlf=false"
                  L" -c core.longpaths=true " + args;
    if (echo) logw(L"> git " + args);
    return runProcess(cmd, inRepo ? g_cfg.repoDir : L"", out, echo);
}

// ============================================================ git helpers
static wstring authUrl() {
    const wstring https = L"https://";
    if (!g_cfg.token.empty() && g_cfg.url.compare(0, https.size(), https) == 0)
        return https + L"x-access-token:" + g_cfg.token + L"@" + g_cfg.url.substr(https.size());
    return g_cfg.url;
}
static wstring gitOut(const wstring& args) {
    wstring o;
    if (git(args, &o, false) != 0) return L"";
    return trimW(o);
}
static wstring branchName() {
    wstring b = gitOut(L"symbolic-ref --short HEAD");
    return b.empty() ? L"main" : b;
}
static wstring worldCommit() { // commit cuoi cung dong vao thu muc world
    return gitOut(L"rev-list -1 HEAD -- " + q(g_cfg.cluster));
}
static wstring me() { return g_cfg.name; }

// Dua repo local ve dung nhu remote (repo local chi la ban sao do tool quan ly).
static bool syncMirror(bool& remoteExists) {
    remoteExists = false;
    if (!fs::exists(fs::path(g_cfg.repoDir) / L".git")) {
        logw(L"Chưa có repo local, đang clone...");
        fs::create_directories(fs::path(g_cfg.repoDir).parent_path());
        if (git(L"clone " + q(authUrl()) + L" " + q(g_cfg.repoDir), nullptr, true, false) != 0) {
            logw(L"Clone thất bại. Kiểm tra URL repo, token và quyền truy cập.");
            return false;
        }
    }
    git(L"remote set-url origin " + q(authUrl()), nullptr, false);
    if (git(L"fetch origin --prune") != 0) {
        logw(L"Fetch thất bại. Kiểm tra mạng / token / quyền truy cập repo.");
        return false;
    }
    wstring br = branchName();
    remoteExists = git(L"rev-parse --verify --quiet origin/" + br, nullptr, false) == 0;
    if (remoteExists && git(L"reset --hard origin/" + br) != 0) return false;
    return true;
}

// 0 = khong co gi de commit, 1 = da commit, -1 = loi
static int commitAll(const wstring& msg) {
    git(L"add -A", nullptr, false);
    if (gitOut(L"status --porcelain").empty()) return 0;
    wstring user = replaceAll(me(), L"\"", L"");
    int rc = git(L"-c user.name=" + q(user) + L" -c user.email=dstsync@local commit -q -m " + q(msg));
    return rc == 0 ? 1 : -1;
}
static bool pushHead() {
    return git(L"push origin HEAD:refs/heads/" + branchName()) == 0;
}

// ============================================================ khoa (lock)
struct Lock { bool exists = false; wstring owner; time_t t = 0; };
static fs::path lockFile() {
    return fs::path(g_cfg.repoDir) / (L"lock_" + g_cfg.cluster + L".txt");
}
static Lock readLock() {
    Lock l;
    std::ifstream f(lockFile(), std::ios::binary);
    if (!f) return l;
    l.exists = true;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("owner=", 0) == 0) l.owner = utf8ToW(line.substr(6));
        else if (line.rfind("time=", 0) == 0) l.t = (time_t)atoll(line.c_str() + 5);
    }
    return l;
}
static void writeLock() {
    std::ofstream f(lockFile(), std::ios::binary | std::ios::trunc);
    f << "owner=" << wToUtf8(me()) << "\ntime=" << (long long)time(nullptr) << "\n";
}

// ============================================================ copy / backup
static bool skipSecret(const fs::path& p) {
    wstring n = lower(p.filename().wstring());
    return n == L"cluster_token.txt" || p.extension() == L".log";
}
static void copyTree(const fs::path& src, const fs::path& dst, bool skipSecrets) {
    fs::remove_all(dst);
    fs::create_directories(dst);
    for (auto& e : fs::recursive_directory_iterator(src)) {
        if (skipSecrets && skipSecret(e.path())) continue;
        fs::path target = dst / fs::relative(e.path(), src);
        if (e.is_directory()) {
            fs::create_directories(target);
        } else {
            if (skipSecrets && e.file_size() > 95ull * 1024 * 1024)
                throw wstring(L"File quá lớn cho GitHub (>95MB): " + e.path().wstring());
            fs::create_directories(target.parent_path());
            fs::copy_file(e.path(), target, fs::copy_options::overwrite_existing);
        }
    }
}
static void pruneBackups() {
    fs::path root(g_cfg.backupDir);
    if (!fs::exists(root)) return;
    std::vector<fs::path> v;
    wstring prefix = g_cfg.cluster + L"_";
    for (auto& e : fs::directory_iterator(root))
        if (e.is_directory() && e.path().filename().wstring().rfind(prefix, 0) == 0) v.push_back(e.path());
    std::sort(v.begin(), v.end());
    while ((int)v.size() > g_cfg.keep) {
        logw(L"Xóa backup cũ: " + v.front().wstring());
        fs::remove_all(v.front());
        v.erase(v.begin());
    }
}
static void makeBackup(const fs::path& src, const wstring& tag) {
    if (!fs::exists(src)) return;
    fs::path dst = fs::path(g_cfg.backupDir) / (g_cfg.cluster + L"_" + stamp() + L"_" + tag);
    copyTree(src, dst, false);
    logw(L"Đã backup: " + dst.wstring());
    pruneBackups();
}

static bool gameRunning(wstring& which) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(s, &pe)) do {
        wstring n = lower(pe.szExeFile);
        if (n.rfind(L"dontstarve_", 0) == 0) { which = pe.szExeFile; found = true; break; }
    } while (Process32NextW(s, &pe));
    CloseHandle(s);
    return found;
}

static void postStatus() {
    Lock l = readLock();
    wstring s;
    if (!fs::exists(fs::path(g_cfg.repoDir) / g_cfg.cluster)) s = L"Repo chưa có world '" + g_cfg.cluster + L"'. ";
    if (!l.exists) s += L"Trạng thái: RẢNH (chưa ai giữ khóa host)";
    else if (l.owner == me()) s += L"Trạng thái: BẠN đang giữ khóa host (từ " + fmtTime(l.t) + L")";
    else s += L"Trạng thái: " + l.owner + L" đang host (từ " + fmtTime(l.t) + L")";
    PostMessageW(g_main, WM_STATUS, 0, (LPARAM) new wstring(s));
}

// ============================================================ cac thao tac chinh
static bool doStatus() {
    bool remote;
    if (!syncMirror(remote)) return false;
    postStatus();
    return true;
}

static bool doPull() {
    wstring which;
    if (gameRunning(which)) {
        logw(L"Game/server đang chạy (" + which + L"). Hãy tắt hẳn trước khi Pull.");
        return false;
    }
    bool remote;
    if (!syncMirror(remote)) return false;
    fs::path world = fs::path(g_cfg.repoDir) / g_cfg.cluster;
    if (!remote || !fs::exists(world)) {
        logw(L"Repo chưa có world '" + g_cfg.cluster + L"'. Hãy Push world lần đầu từ máy đang có world.");
        postStatus();
        return false;
    }

    Lock lk = readLock();
    if (lk.exists && lk.owner != me()) {
        int r = ask(L"World đang bị khóa",
            lk.owner + L" đang giữ khóa host từ " + fmtTime(lk.t) + L".\n\n"
            L"Nếu bạn Pull bây giờ, bạn sẽ chiếm khóa và " + lk.owner +
            L" sẽ gặp xung đột khi Push.\n\nChỉ làm vậy nếu họ đã host xong/đã bỏ dở.\n\nVẫn Pull?",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (r != IDYES) { logw(L"Đã hủy."); return false; }
    } else if (lk.exists && lk.owner == me()) {
        int r = ask(L"Bạn đang giữ khóa",
            L"Bạn đã Pull và đang giữ khóa host. Pull lại sẽ ghi đè save hiện tại trên máy bằng bản trong repo "
            L"(save hiện tại sẽ được backup trước).\n\nTiếp tục?",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (r != IDYES) { logw(L"Đã hủy."); return false; }
    }

    wstring commitBase = worldCommit();

    // Chiếm khóa trước: nếu thất bại thì chưa động gì vào save local.
    writeLock();
    if (commitAll(me() + L" bắt đầu host " + g_cfg.cluster) < 0 || !pushHead()) {
        logw(L"Không chiếm được khóa (có thể ai đó vừa thao tác trước bạn). Đã hủy, save local chưa bị đổi.");
        wstring br = branchName();
        git(L"fetch origin", nullptr, false);
        git(L"reset --hard origin/" + br, nullptr, false);
        postStatus();
        return false;
    }

    fs::path save = fs::path(g_cfg.saveRoot) / g_cfg.cluster;
    makeBackup(save, L"pull");

    std::string token; // giữ token server riêng của máy này
    fs::path tokenFile = save / L"cluster_token.txt";
    if (fs::exists(tokenFile)) {
        std::ifstream f(tokenFile, std::ios::binary);
        token.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    copyTree(world, save, true);
    if (!token.empty()) { std::ofstream f(tokenFile, std::ios::binary); f << token; }

    g_cfg.lastSynced = commitBase;
    saveCfg();
    logw(L"Pull xong. Bạn đang giữ khóa, có thể mở game/server để host.");
    postStatus();
    return true;
}

static bool doPush() {
    wstring which;
    if (gameRunning(which)) {
        logw(L"Game/server đang chạy (" + which + L"). TẮT HẲN game rồi mới Push để tránh hỏng save.");
        return false;
    }
    fs::path save = fs::path(g_cfg.saveRoot) / g_cfg.cluster;
    if (!fs::exists(save)) {
        logw(L"Không thấy thư mục save: " + save.wstring());
        return false;
    }
    bool remote;
    if (!syncMirror(remote)) return false;

    fs::path world = fs::path(g_cfg.repoDir) / g_cfg.cluster;
    Lock lk = readLock();
    bool otherLock = lk.exists && lk.owner != me();
    bool diverged = remote && fs::exists(world) && worldCommit() != g_cfg.lastSynced;

    bool separate = false;
    if (otherLock || diverged) {
        wstring why = L"Phát hiện xung đột:\n";
        if (otherLock) why += L"  - " + lk.owner + L" đang giữ khóa host (từ " + fmtTime(lk.t) + L").\n";
        if (diverged) why += L"  - World trên repo đã bị thay đổi kể từ lần Pull cuối của bạn.\n";
        why += L"\nChọn cách xử lý:\n\n"
               L"  CÓ (Yes)   = Ghi đè bản trên repo bằng bản của bạn (bản repo cũ được backup về máy)\n"
               L"  KHÔNG (No) = Đẩy bản của bạn vào thư mục riêng trên repo, KHÔNG ghi đè gì cả\n"
               L"  Hủy        = Không làm gì";
        int r = ask(L"Xung đột khi Push", why, MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
        if (r == IDCANCEL) { logw(L"Đã hủy Push."); return false; }
        if (r == IDYES) makeBackup(world, L"remote-truoc-khi-ghi-de");
        else separate = true;
    }

    fs::path gi = fs::path(g_cfg.repoDir) / L".gitignore";
    if (!fs::exists(gi)) { std::ofstream f(gi, std::ios::binary); f << "cluster_token.txt\n*.log\n"; }

    if (separate) {
        wstring dname = g_cfg.cluster + L"_conflict_" + me() + L"_" + stamp();
        copyTree(save, fs::path(g_cfg.repoDir) / dname, true);
        if (commitAll(me() + L" đẩy bản xung đột: " + dname) < 0 || !pushHead()) {
            logw(L"Push bản riêng thất bại.");
            return false;
        }
        logw(L"Đã đẩy bản của bạn vào thư mục '" + dname + L"' trên repo. Bản chính không bị ảnh hưởng.");
        logw(L"Muốn dùng bản này: đổi tên thư mục đó thành '" + g_cfg.cluster + L"' trên repo (hoặc Push ghi đè sau).");
        postStatus();
        return true;
    }

    copyTree(save, world, true);
    if (fs::exists(lockFile())) fs::remove(lockFile());
    int c = commitAll(me() + L" cập nhật " + g_cfg.cluster);
    if (c < 0) { logw(L"Commit thất bại."); return false; }
    if (c == 0) {
        logw(L"Không có thay đổi nào so với repo.");
    } else if (!pushHead()) {
        logw(L"Push bị từ chối (có người vừa push). Đã đồng bộ lại; hãy bấm Push lần nữa để kiểm tra xung đột.");
        git(L"fetch origin", nullptr, false);
        git(L"reset --hard origin/" + branchName(), nullptr, false);
        postStatus();
        return false;
    }
    g_cfg.lastSynced = worldCommit();
    saveCfg();
    logw(L"Push xong, đã nhả khóa host.");
    postStatus();
    return true;
}

static bool doGetGit() {
    fs::path ps = exeDir() / L"get-git.ps1";
    if (!fs::exists(ps)) { logw(L"Không thấy get-git.ps1 cạnh dstsync.exe."); return false; }
    logw(L"Đang tải PortableGit (~60MB), vui lòng đợi...");
    int rc = runProcess(L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + q(ps.wstring()),
                        exeDir().wstring(), nullptr, true);
    g_git = findGit();
    PostMessageW(g_main, WM_COMMAND, MAKEWPARAM(ID_LABEL_GIT, 0), 0); // cap nhat nhan git
    if (rc != 0 || g_git.empty()) { logw(L"Tải Git thất bại. Bạn có thể cài Git for Windows thủ công."); return false; }
    logw(L"Đã có Git: " + g_git);
    return true;
}

static DWORD WINAPI workerProc(LPVOID p) {
    int op = (int)(INT_PTR)p;
    bool ok = false;
    try {
        switch (op) {
            case OP_PULL: ok = doPull(); break;
            case OP_PUSH: ok = doPush(); break;
            case OP_STATUS: ok = doStatus(); break;
            case OP_GETGIT: ok = doGetGit(); if (ok && cfgComplete()) doStatus(); break;
        }
    } catch (const wstring& e) {
        logw(L"Lỗi: " + e);
    } catch (const std::exception& e) {
        logw(L"Lỗi: " + acpToW(e.what()));
    }
    if (op == OP_PULL || op == OP_PUSH) logw(ok ? L"=== HOÀN TẤT ===" : L"=== KHÔNG THÀNH CÔNG ===");
    PostMessageW(g_main, WM_DONE, 0, 0);
    return 0;
}

static void setButtonsEnabled(bool en) {
    for (HWND b : g_btns) if (b) EnableWindow(b, en);
}
static void startOp(Op op) {
    if (g_busy.exchange(true)) return;
    if (op != OP_GETGIT) {
        if (!cfgComplete()) { g_busy = false; MessageBoxW(g_main, L"Hãy hoàn tất Cài đặt trước.", L"DST Sync", MB_ICONINFORMATION); return; }
        if (g_git.empty()) { g_busy = false; MessageBoxW(g_main, L"Chưa tìm thấy Git. Bấm 'Tải Git' hoặc cài Git for Windows.", L"DST Sync", MB_ICONWARNING); return; }
    }
    setButtonsEnabled(false);
    HANDLE h = CreateThread(nullptr, 0, workerProc, (LPVOID)(INT_PTR)op, 0, nullptr);
    if (h) CloseHandle(h); else { g_busy = false; setButtonsEnabled(true); }
}

// ============================================================ man hinh cai dat
enum { ID_S_BASE = 300, ID_S_BROWSE = 320, ID_S_SCAN = 340, ID_S_SAVE, ID_S_CANCEL };
static HWND sCtl[8];
static const wchar_t* kLabels[8] = {
    L"Tên hiển thị của bạn", L"URL repo GitHub", L"Token GitHub (tùy chọn)", L"Thư mục repo trên máy",
    L"Thư mục save của game", L"Tên world (cluster)", L"Thư mục backup", L"Số bản backup giữ lại"};

static wstring getText(HWND h) {
    int n = GetWindowTextLengthW(h);
    wstring s(n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    return s;
}
static BOOL CALLBACK fontEnumProc(HWND c, LPARAM) {
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return TRUE;
}
static void applyFont(HWND h) {
    SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    EnumChildWindows(h, fontEnumProc, 0);
}
static void scanClusters() {
    SendMessageW(sCtl[5], CB_RESETCONTENT, 0, 0);
    fs::path root(trimW(getText(sCtl[4])));
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    for (auto& e : fs::directory_iterator(root, ec))
        if (e.is_directory() && (fs::exists(e.path() / L"cluster.ini") || fs::exists(e.path() / L"Master")))
            SendMessageW(sCtl[5], CB_ADDSTRING, 0, (LPARAM)e.path().filename().c_str());
}
static void browseInto(HWND owner, HWND edit, const wchar_t* title) {
    BROWSEINFOW bi{};
    bi.hwndOwner = owner; bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (LPITEMIDLIST pidl = SHBrowseForFolderW(&bi)) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) SetWindowTextW(edit, path);
        CoTaskMemFree(pidl);
    }
}
static void saveSetup(HWND hwnd) {
    Cfg n = g_cfg;
    auto clean = [](wstring s) { s = replaceAll(s, L"\"", L""); s = replaceAll(s, L"\r", L""); return trimW(replaceAll(s, L"\n", L"")); };
    n.name = clean(getText(sCtl[0]));
    n.url = trimW(getText(sCtl[1]));
    n.token = trimW(getText(sCtl[2]));
    n.repoDir = cleanPath(getText(sCtl[3]));
    n.saveRoot = cleanPath(getText(sCtl[4]));
    n.cluster = clean(getText(sCtl[5]));
    n.backupDir = cleanPath(getText(sCtl[6]));
    n.keep = std::clamp((int)wcstol(getText(sCtl[7]).c_str(), nullptr, 10), 1, 50);
    if (n.name.empty() || n.url.empty() || n.repoDir.empty() || n.saveRoot.empty() || n.cluster.empty() || n.backupDir.empty()) {
        MessageBoxW(hwnd, L"Vui lòng điền đủ các ô (trừ Token).", L"Thiếu thông tin", MB_ICONWARNING);
        return;
    }
    if (n.cluster.find_first_of(L"\\/:*?<>|") != wstring::npos) {
        MessageBoxW(hwnd, L"Tên world không được chứa ký tự đặc biệt hoặc dấu gạch chéo.", L"Sai tên world", MB_ICONWARNING);
        return;
    }
    if (n.url.rfind(L"https://", 0) != 0 && n.url.rfind(L"git@", 0) != 0 && n.url.rfind(L"http://", 0) != 0)
        if (MessageBoxW(hwnd, L"URL repo trông không đúng (thường bắt đầu bằng https://github.com/...). Vẫn lưu?", L"Kiểm tra URL", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (n.cluster != g_cfg.cluster || n.url != g_cfg.url) n.lastSynced.clear();
    g_cfg = n;
    saveCfg();
    DestroyWindow(hwnd);
    PostMessageW(g_main, WM_INIT_APP, 1, 0);
}

static LRESULT CALLBACK SetupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        const wstring vals[8] = {g_cfg.name, g_cfg.url, g_cfg.token, g_cfg.repoDir, g_cfg.saveRoot,
                                 g_cfg.cluster, g_cfg.backupDir, std::to_wstring(g_cfg.keep)};
        for (int i = 0; i < 8; ++i) {
            int y = 14 + i * 34;
            CreateWindowW(L"STATIC", kLabels[i], WS_CHILD | WS_VISIBLE, 12, y + 4, 170, 22, hwnd, nullptr, g_hInst, nullptr);
            if (i == 5) {
                sCtl[i] = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL,
                                        185, y, 310, 220, hwnd, (HMENU)(INT_PTR)(ID_S_BASE + i), g_hInst, nullptr);
            } else {
                DWORD st = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (i == 2 ? ES_PASSWORD : 0);
                sCtl[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", vals[i].c_str(), st, 185, y, 310, 24, hwnd,
                                          (HMENU)(INT_PTR)(ID_S_BASE + i), g_hInst, nullptr);
            }
            if (i == 3 || i == 4 || i == 6)
                CreateWindowW(L"BUTTON", L"Chọn...", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 502, y, 72, 24, hwnd, (HMENU)(INT_PTR)(ID_S_BROWSE + i), g_hInst, nullptr);
            if (i == 5)
                CreateWindowW(L"BUTTON", L"Quét", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 502, y, 72, 24, hwnd, (HMENU)(INT_PTR)ID_S_SCAN, g_hInst, nullptr);
        }
        SetWindowTextW(sCtl[5], g_cfg.cluster.c_str());
        scanClusters();
        SetWindowTextW(sCtl[5], g_cfg.cluster.c_str());
        CreateWindowW(L"STATIC",
            L"Token: tạo ở GitHub > Settings > Developer settings > Fine-grained tokens, chỉ cấp quyền\n"
            L"Contents: Read and write cho đúng repo này. Để trống nếu máy đã đăng nhập Git sẵn.",
            WS_CHILD | WS_VISIBLE, 12, 14 + 8 * 34 + 2, 565, 40, hwnd, nullptr, g_hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Lưu", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 390, 14 + 8 * 34 + 52, 90, 30, hwnd, (HMENU)(INT_PTR)ID_S_SAVE, g_hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Hủy", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 486, 14 + 8 * 34 + 52, 90, 30, hwnd, (HMENU)(INT_PTR)ID_S_CANCEL, g_hInst, nullptr);
        applyFont(hwnd);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_S_SAVE) saveSetup(hwnd);
        else if (id == ID_S_CANCEL || id == IDCANCEL) DestroyWindow(hwnd);
        else if (id == ID_S_SCAN) scanClusters();
        else if (id == ID_S_BROWSE + 3) browseInto(hwnd, sCtl[3], L"Chọn thư mục chứa bản sao repo");
        else if (id == ID_S_BROWSE + 4) { browseInto(hwnd, sCtl[4], L"Chọn thư mục save (DoNotStarveTogether)"); scanClusters(); }
        else if (id == ID_S_BROWSE + 6) browseInto(hwnd, sCtl[6], L"Chọn thư mục lưu backup");
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        g_setup = nullptr;
        EnableWindow(g_main, TRUE);
        SetForegroundWindow(g_main);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
static void showSetup() {
    if (g_setup || g_busy) return;
    RECT r; GetWindowRect(g_main, &r);
    int w = 600, h = 14 + 8 * 34 + 52 + 30 + 60;
    g_setup = CreateWindowExW(WS_EX_DLGMODALFRAME, L"DstSetup", L"Cài đặt DST Sync",
                              WS_POPUP | WS_CAPTION | WS_SYSMENU, r.left + 20, r.top + 20, w, h,
                              g_main, nullptr, g_hInst, nullptr);
    EnableWindow(g_main, FALSE);
    ShowWindow(g_setup, SW_SHOW);
}

// ============================================================ cua so chinh
static void refreshGitLabel() {
    SetWindowTextW(g_lblGit, g_git.empty() ? L"Git: CHƯA CÓ  (bấm 'Tải Git' để tự tải bản đi kèm)"
                                           : (L"Git: " + g_git).c_str());
    EnableWindow(g_btns[5], g_git.empty() && !g_busy);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_main = hwnd;
        g_lblStatus = CreateWindowW(L"STATIC", L"Trạng thái: chưa kiểm tra", WS_CHILD | WS_VISIBLE, 12, 12, 596, 22, hwnd, (HMENU)ID_LABEL_STATUS, g_hInst, nullptr);
        struct B { const wchar_t* t; int id; int x; int w; } bs[] = {
            {L"Pull (lấy world)", ID_PULL, 12, 128}, {L"Push (đẩy world)", ID_PUSH, 146, 128},
            {L"Làm mới trạng thái", ID_STATUS, 280, 132}, {L"Cài đặt", ID_SETTINGS, 418, 70},
            {L"Backup", ID_BACKUPS, 494, 56}, {L"Tải Git", ID_GETGIT, 556, 52}};
        for (int i = 0; i < 6; ++i)
            g_btns[i] = CreateWindowW(L"BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP, bs[i].x, 42, bs[i].w, 34, hwnd, (HMENU)(INT_PTR)bs[i].id, g_hInst, nullptr);
        g_log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                12, 88, 596, 330, hwnd, (HMENU)ID_LOG, g_hInst, nullptr);
        SendMessageW(g_log, EM_SETLIMITTEXT, 1 << 24, 0);
        g_lblGit = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, 12, 426, 596, 22, hwnd, (HMENU)ID_LABEL_GIT, g_hInst, nullptr);
        applyFont(hwnd);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_PULL: startOp(OP_PULL); break;
        case ID_PUSH: startOp(OP_PUSH); break;
        case ID_STATUS: startOp(OP_STATUS); break;
        case ID_SETTINGS: showSetup(); break;
        case ID_GETGIT: startOp(OP_GETGIT); break;
        case ID_LABEL_GIT: refreshGitLabel(); break; // worker bao da tim lai git
        case ID_BACKUPS: {
            std::error_code ec;
            fs::create_directories(g_cfg.backupDir, ec);
            ShellExecuteW(hwnd, L"open", g_cfg.backupDir.c_str(), nullptr, nullptr, SW_SHOW);
            break;
        }
        }
        return 0;
    case WM_LOG: {
        wstring* s = (wstring*)lp;
        int len = GetWindowTextLengthW(g_log);
        SendMessageW(g_log, EM_SETSEL, len, len);
        SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)s->c_str());
        SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
        delete s;
        return 0;
    }
    case WM_STATUS: {
        wstring* s = (wstring*)lp;
        SetWindowTextW(g_lblStatus, s->c_str());
        delete s;
        return 0;
    }
    case WM_ASK: {
        AskData* a = (AskData*)lp;
        a->result = MessageBoxW(hwnd, a->text.c_str(), a->title.c_str(), a->flags);
        return 0;
    }
    case WM_DONE:
        g_busy = false;
        setButtonsEnabled(true);
        refreshGitLabel();
        return 0;
    case WM_INIT_APP:
        refreshGitLabel();
        if (wp == 0 && !cfgComplete()) { showSetup(); return 0; }
        if (g_git.empty()) {
            if (ask(L"Chưa có Git", L"Máy này chưa có Git. Tự tải bản PortableGit (~60MB) về đặt cạnh chương trình?",
                    MB_YESNO | MB_ICONQUESTION) == IDYES)
                startOp(OP_GETGIT);
        } else if (cfgComplete()) {
            startOp(OP_STATUS);
        }
        return 0;
    case WM_CLOSE:
        if (g_busy) {
            MessageBoxW(hwnd, L"Đang thực hiện thao tác. Hãy đợi xong rồi hãy đóng để tránh hỏng save.", L"DST Sync", MB_ICONWARNING);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    g_hInst = hInst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetEnvironmentVariableW(L"GIT_TERMINAL_PROMPT", L"0"); // khong de git treo vi cho nhap mat khau
    defaultCfg();
    loadCfg();
    g_git = findGit();

    g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    WNDCLASSW wc{};
    wc.hInstance = hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DstMain"; wc.lpfnWndProc = MainProc;
    RegisterClassW(&wc);
    wc.lpszClassName = L"DstSetup"; wc.lpfnWndProc = SetupProc;
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc{0, 0, 620, 458};
    AdjustWindowRect(&rc, style, FALSE);
    HWND hw = CreateWindowW(L"DstMain", L"DST Sync - đồng bộ world qua GitHub", style, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hw, nShow);
    UpdateWindow(hw);
    PostMessageW(hw, WM_INIT_APP, 0, 0);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (g_setup && IsDialogMessageW(g_setup, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
