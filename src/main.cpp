#include <windows.h>
#include <shellapi.h>
#include <powrprof.h>

#include <cstring>
#include <cwchar>

namespace {

constexpr wchar_t kClassName[] = L"NoSleepLidWindow";
constexpr wchar_t kAppName[] = L"NoSleep Lid";
constexpr wchar_t kSettingsKey[] = L"Software\\NoSleepLid";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"NoSleepLid";
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT ID_TRAY = 1;
constexpr UINT_PTR IDT_UPTIME = 1;
constexpr int HOTKEY_TOGGLE = 1;

constexpr int IDM_TOGGLE = 100;
constexpr int IDM_SHOW = 101;
constexpr int IDM_EXIT = 102;
constexpr int IDC_ENABLED = 200;
constexpr int IDC_AC_OFF = 201;
constexpr int IDC_AC_SLEEP = 202;
constexpr int IDC_AC_HIBERNATE = 203;
constexpr int IDC_DC_OFF = 204;
constexpr int IDC_DC_SLEEP = 205;
constexpr int IDC_DC_HIBERNATE = 206;
constexpr int IDC_LOCK = 207;
constexpr int IDC_KILL_PROCS = 208;
constexpr int IDC_AUTOSTART = 209;
constexpr int IDC_START_MIN = 210;
constexpr int IDC_NOTIFY = 211;
constexpr int IDC_RESTORE = 212;
constexpr int IDC_PROCESS_LIST = 213;
constexpr int IDC_SLEEP_COUNTER = 214;
constexpr int IDC_LID_COUNTER = 215;
constexpr int IDC_UPTIME = 216;
constexpr int IDC_CURRENT = 217;
constexpr int IDC_LOG = 218;

// 0 = do nothing, 1 = sleep, 2 = hibernate.
constexpr DWORD kPowerNone = 0;
constexpr DWORD kPowerSleep = 1;
constexpr DWORD kPowerHibernate = 2;
constexpr GUID kGuidLidSwitchStateChange{
    0xBA3E0F4D, 0xB817, 0x4094, {0xA2, 0xD1, 0xD5, 0x63, 0x79, 0xE6, 0xA0, 0xF3}};
constexpr GUID kGuidSubButtons{
    0x4F971E89, 0xEEBD, 0x4455, {0xA8, 0xDE, 0x9E, 0x59, 0x04, 0x0E, 0x73, 0x47}};
constexpr GUID kGuidLidAction{
    0x5CA83367, 0x6E45, 0x459F, {0xA2, 0x7B, 0x47, 0x6B, 0x1D, 0x01, 0xC9, 0x36}};

struct State {
  HWND hwnd{};
  HWND enabled{};
  HWND acOff{};
  HWND acSleep{};
  HWND acHibernate{};
  HWND dcOff{};
  HWND dcSleep{};
  HWND dcHibernate{};
  HWND lock{};
  HWND killProcs{};
  HWND autostartBox{};
  HWND startMinimized{};
  HWND notify{};
  HWND restore{};
  HWND processList{};
  HWND sleepCounter{};
  HWND lidCounter{};
  HWND uptime{};
  HWND current{};
  HWND log{};
  HICON iconOn{};
  HICON iconOff{};
  HPOWERNOTIFY lidNotify{};
  bool lidActionEnabled = true;
  DWORD acAction = kPowerSleep;
  DWORD dcAction = kPowerSleep;
  DWORD originalAcAction = kPowerSleep;
  DWORD originalDcAction = kPowerSleep;
  bool haveOriginalActions = false;
  bool lockOnClose = false;
  bool killProcessesOnClose = false;
  bool autostartEnabled = false;
  bool startMinimizedEnabled = true;
  bool notifyEnabled = true;
  bool restoreOnExit = false;
  DWORD sleepCount = 0;
  DWORD lidCloseCount = 0;
  DWORD uptimeTicks = 0;
  wchar_t processListText[512] = L"chrome.exe";
  bool polish = false;
  bool startHidden = false;
};

State g;

enum class TextId {
  TraySleepOn,
  TraySleepOff,
  DisableLidSleep,
  EnableLidSleep,
  ShowWindow,
  Exit,
  LidActionEnabled,
  AcLabel,
  DcLabel,
  Off,
  Sleep,
  Hibernate,
  LockOnClose,
  KillProcessesOnClose,
  Autostart,
  StartMinimized,
  Notify,
  Restore,
  ProcessList,
  SleepCounter,
  LidCounter,
  Uptime,
  Current,
  LogStart,
  NotifyOn,
  NotifyOff,
};

const wchar_t* Text(TextId id) {
  if (g.polish) {
    switch (id) {
      case TextId::TraySleepOn: return L"akcja klapy włączona";
      case TextId::TraySleepOff: return L"akcja klapy wyłączona";
      case TextId::DisableLidSleep: return L"Wyłącz akcję klapy";
      case TextId::EnableLidSleep: return L"Włącz akcję klapy";
      case TextId::ShowWindow: return L"Pokaż okno";
      case TextId::Exit: return L"Zamknij";
      case TextId::LidActionEnabled: return L"Akcja po zamknięciu klapy włączona";
      case TextId::AcLabel: return L"Zasilanie:";
      case TextId::DcLabel: return L"Bateria:";
      case TextId::Off: return L"brak";
      case TextId::Sleep: return L"uśpij";
      case TextId::Hibernate: return L"hibernuj";
      case TextId::LockOnClose: return L"Zablokuj ekran po zamknięciu klapy";
      case TextId::KillProcessesOnClose: return L"Zabij procesy po zamknięciu klapy";
      case TextId::Autostart: return L"Autostart z systemem";
      case TextId::StartMinimized: return L"Startuj zminimalizowany do tray";
      case TextId::Notify: return L"Powiadomienia tray po zmianie stanu";
      case TextId::Restore: return L"Przywróć poprzednie ustawienia przy wyjściu";
      case TextId::ProcessList: return L"Procesy do zabicia:";
      case TextId::SleepCounter: return L"Uśpienia/hibernacje od startu systemu: %lu";
      case TextId::LidCounter: return L"Zamknięcia klapy od startu aplikacji: %lu";
      case TextId::Uptime: return L"Uptime systemu: %llud %02llu:%02llu:%02llu";
      case TextId::Current: return L"Aktualnie: AC=%s, bateria=%s";
      case TextId::LogStart: return L"Log zdarzeń";
      case TextId::NotifyOn: return L"Akcja klapy włączona";
      case TextId::NotifyOff: return L"Akcja klapy wyłączona";
    }
  }

  switch (id) {
    case TextId::TraySleepOn: return L"lid action enabled";
    case TextId::TraySleepOff: return L"lid action disabled";
    case TextId::DisableLidSleep: return L"Disable lid action";
    case TextId::EnableLidSleep: return L"Enable lid action";
    case TextId::ShowWindow: return L"Show window";
    case TextId::Exit: return L"Exit";
    case TextId::LidActionEnabled: return L"Lid close action enabled";
    case TextId::AcLabel: return L"AC power:";
    case TextId::DcLabel: return L"Battery:";
    case TextId::Off: return L"off";
    case TextId::Sleep: return L"sleep";
    case TextId::Hibernate: return L"hibernate";
    case TextId::LockOnClose: return L"Lock screen on lid close";
    case TextId::KillProcessesOnClose: return L"Kill processes on lid close";
    case TextId::Autostart: return L"Start with Windows";
    case TextId::StartMinimized: return L"Start minimized to tray";
    case TextId::Notify: return L"Tray notifications on state changes";
    case TextId::Restore: return L"Restore previous settings on exit";
    case TextId::ProcessList: return L"Processes to kill:";
    case TextId::SleepCounter: return L"Sleeps/hibernations since system boot: %lu";
    case TextId::LidCounter: return L"Lid closes since app start: %lu";
    case TextId::Uptime: return L"System uptime: %llud %02llu:%02llu:%02llu";
    case TextId::Current: return L"Current: AC=%s, battery=%s";
    case TextId::LogStart: return L"Event log";
    case TextId::NotifyOn: return L"Lid action enabled";
    case TextId::NotifyOff: return L"Lid action disabled";
  }
  return L"";
}

const wchar_t* ActionName(DWORD action) {
  if (action == kPowerSleep) return Text(TextId::Sleep);
  if (action == kPowerHibernate) return Text(TextId::Hibernate);
  return Text(TextId::Off);
}

DWORD ReadDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD fallback) {
  HKEY key{};
  DWORD value = fallback;
  DWORD size = sizeof(value);
  if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
  }
  return value;
}

void WriteDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD value) {
  HKEY key{};
  if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
  }
}

void ReadString(HKEY root, const wchar_t* path, const wchar_t* name, wchar_t* value, DWORD chars) {
  HKEY key{};
  DWORD bytes = chars * sizeof(wchar_t);
  if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(value), &bytes);
    value[chars - 1] = 0;
    RegCloseKey(key);
  }
}

void WriteString(HKEY root, const wchar_t* path, const wchar_t* name, const wchar_t* value) {
  HKEY key{};
  if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                   static_cast<DWORD>((lstrlenW(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
  }
}

void Log(const wchar_t* text) {
  if (!g.log) return;
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t line[768]{};
  std::swprintf(line, ARRAYSIZE(line), L"[%02u:%02u:%02u] %s\r\n", st.wHour, st.wMinute, st.wSecond, text);
  int len = GetWindowTextLengthW(g.log);
  SendMessageW(g.log, EM_SETSEL, len, len);
  SendMessageW(g.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line));
}

void LoadSettings() {
  g.polish = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_POLISH;
  g.lidActionEnabled = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"LidActionEnabled", 1) != 0;
  g.acAction = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"AcAction", kPowerSleep);
  g.dcAction = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"DcAction", kPowerSleep);
  if (g.acAction > kPowerHibernate) g.acAction = kPowerSleep;
  if (g.dcAction > kPowerHibernate) g.dcAction = kPowerSleep;
  g.lockOnClose = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"LockOnClose", 0) != 0;
  g.killProcessesOnClose = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"KillProcessesOnClose", 0) != 0;
  g.startMinimizedEnabled = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"StartMinimized", 1) != 0;
  g.notifyEnabled = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"Notify", 1) != 0;
  g.restoreOnExit = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"RestoreOnExit", 0) != 0;
  ReadString(HKEY_CURRENT_USER, kSettingsKey, L"ProcessList", g.processListText, ARRAYSIZE(g.processListText));

  HKEY key{};
  g.autostartEnabled = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) == ERROR_SUCCESS;
  if (g.autostartEnabled) {
    g.autostartEnabled = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);
  }
}

void SaveSettings() {
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"LidActionEnabled", g.lidActionEnabled ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"AcAction", g.acAction);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"DcAction", g.dcAction);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"LockOnClose", g.lockOnClose ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"KillProcessesOnClose", g.killProcessesOnClose ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"StartMinimized", g.startMinimizedEnabled ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"Notify", g.notifyEnabled ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"RestoreOnExit", g.restoreOnExit ? 1 : 0);
  WriteString(HKEY_CURRENT_USER, kSettingsKey, L"ProcessList", g.processListText);
}

HICON MakeIcon(COLORREF color) {
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  HBITMAP colorBmp = CreateCompatibleBitmap(screen, 16, 16);
  HBITMAP maskBmp = CreateBitmap(16, 16, 1, 1, nullptr);
  HGDIOBJ old = SelectObject(dc, colorBmp);
  RECT rect{0, 0, 16, 16};
  HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
  FillRect(dc, &rect, bg);
  DeleteObject(bg);
  HBRUSH brush = CreateSolidBrush(color);
  HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, brush));
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(30, 30, 30));
  HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
  Ellipse(dc, 2, 2, 14, 14);
  SelectObject(dc, oldPen);
  SelectObject(dc, oldBrush);
  DeleteObject(pen);
  DeleteObject(brush);
  SelectObject(dc, old);
  ICONINFO ii{};
  ii.fIcon = TRUE;
  ii.hbmColor = colorBmp;
  ii.hbmMask = maskBmp;
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(colorBmp);
  DeleteObject(maskBmp);
  DeleteDC(dc);
  ReleaseDC(nullptr, screen);
  return icon;
}

void UpdateTray() {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = g.hwnd;
  nid.uID = ID_TRAY;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAY;
  nid.hIcon = g.lidActionEnabled ? g.iconOn : g.iconOff;
  std::swprintf(nid.szTip, ARRAYSIZE(nid.szTip), L"%s: %s", kAppName,
                g.lidActionEnabled ? Text(TextId::TraySleepOn) : Text(TextId::TraySleepOff));
  Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void ShowNotification(const wchar_t* msg) {
  if (!g.notifyEnabled) return;
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = g.hwnd;
  nid.uID = ID_TRAY;
  nid.uFlags = NIF_INFO;
  lstrcpynW(nid.szInfoTitle, kAppName, ARRAYSIZE(nid.szInfoTitle));
  lstrcpynW(nid.szInfo, msg, ARRAYSIZE(nid.szInfo));
  nid.dwInfoFlags = NIIF_INFO;
  Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void AddTray() {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = g.hwnd;
  nid.uID = ID_TRAY;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAY;
  nid.hIcon = g.lidActionEnabled ? g.iconOn : g.iconOff;
  lstrcpynW(nid.szTip, kAppName, ARRAYSIZE(nid.szTip));
  Shell_NotifyIconW(NIM_ADD, &nid);
  nid.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &nid);
  UpdateTray();
}

void RemoveTray() {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = g.hwnd;
  nid.uID = ID_TRAY;
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

void RunHidden(const wchar_t* command) {
  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  wchar_t buffer[768]{};
  lstrcpynW(buffer, command, ARRAYSIZE(buffer));
  if (CreateProcessW(nullptr, buffer, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
}

bool RunCapture(const wchar_t* command, char* output, DWORD outputSize) {
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE readPipe{};
  HANDLE writePipe{};
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.wShowWindow = SW_HIDE;
  wchar_t buffer[1024]{};
  lstrcpynW(buffer, command, ARRAYSIZE(buffer));
  bool ok = CreateProcessW(nullptr, buffer, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != FALSE;
  CloseHandle(writePipe);
  if (!ok) {
    CloseHandle(readPipe);
    return false;
  }

  DWORD total = 0;
  for (;;) {
    DWORD read = 0;
    if (!ReadFile(readPipe, output + total, outputSize - total - 1, &read, nullptr) || read == 0) break;
    total += read;
    if (total + 1 >= outputSize) break;
  }
  output[total] = 0;

  WaitForSingleObject(pi.hProcess, 3000);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(readPipe);
  return true;
}

DWORD CountXmlEvents(const char* text) {
  DWORD count = 0;
  const char* p = text;
  while ((p = std::strstr(p, "<Event ")) != nullptr) {
    ++count;
    p += 7;
  }
  return count;
}

DWORD QuerySleepCountSinceBoot() {
  ULONGLONG uptimeMs = GetTickCount64();
  wchar_t cmd[768]{};
  std::swprintf(
      cmd, ARRAYSIZE(cmd),
      L"wevtutil qe System /q:\"*[System[Provider[@Name='Microsoft-Windows-Kernel-Power'] and EventID=42 and TimeCreated[timediff(@SystemTime) <= %llu]]]\" /f:xml",
      uptimeMs);
  char output[32768]{};
  if (!RunCapture(cmd, output, sizeof(output))) return g.sleepCount;
  return CountXmlEvents(output);
}

void QueryCurrentActions(DWORD* ac, DWORD* dc) {
  GUID* scheme = nullptr;
  if (PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS && scheme) {
    PowerReadACValueIndex(nullptr, scheme, &kGuidSubButtons, &kGuidLidAction, ac);
    PowerReadDCValueIndex(nullptr, scheme, &kGuidSubButtons, &kGuidLidAction, dc);
    LocalFree(scheme);
  }
}

void CaptureOriginalActions() {
  g.originalAcAction = g.acAction;
  g.originalDcAction = g.dcAction;
  QueryCurrentActions(&g.originalAcAction, &g.originalDcAction);
  g.haveOriginalActions = true;
}

void ApplyPowerSettings() {
  DWORD ac = g.lidActionEnabled ? g.acAction : kPowerNone;
  DWORD dc = g.lidActionEnabled ? g.dcAction : kPowerNone;
  wchar_t cmd[256]{};
  std::swprintf(cmd, ARRAYSIZE(cmd), L"powercfg /setacvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION %lu", ac);
  RunHidden(cmd);
  std::swprintf(cmd, ARRAYSIZE(cmd), L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION %lu", dc);
  RunHidden(cmd);
  RunHidden(L"powercfg /setactive SCHEME_CURRENT");
}

void RestoreOriginalSettings() {
  if (!g.restoreOnExit || !g.haveOriginalActions) return;
  DWORD savedAc = g.acAction;
  DWORD savedDc = g.dcAction;
  bool savedEnabled = g.lidActionEnabled;
  g.acAction = g.originalAcAction;
  g.dcAction = g.originalDcAction;
  g.lidActionEnabled = true;
  ApplyPowerSettings();
  g.acAction = savedAc;
  g.dcAction = savedDc;
  g.lidActionEnabled = savedEnabled;
}

void SetAutostart(bool enabled) {
  HKEY key{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
  if (enabled) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    wchar_t quoted[MAX_PATH + 32]{};
    std::swprintf(quoted, ARRAYSIZE(quoted), L"\"%s\"%s", path, g.startMinimizedEnabled ? L" --minimized" : L"");
    RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted),
                   static_cast<DWORD>((lstrlenW(quoted) + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, kRunValue);
  }
  RegCloseKey(key);
  g.autostartEnabled = enabled;
}

void UpdateCurrentAction() {
  DWORD ac = g.acAction;
  DWORD dc = g.dcAction;
  QueryCurrentActions(&ac, &dc);
  wchar_t text[160]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::Current), ActionName(ac), ActionName(dc));
  SetWindowTextW(g.current, text);
}

void UpdateSleepCount() {
  g.sleepCount = QuerySleepCountSinceBoot();
  wchar_t text[128]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::SleepCounter), g.sleepCount);
  SetWindowTextW(g.sleepCounter, text);
}

void UpdateUptime() {
  ULONGLONG seconds = GetTickCount64() / 1000;
  ULONGLONG days = seconds / 86400;
  seconds %= 86400;
  ULONGLONG hours = seconds / 3600;
  seconds %= 3600;
  ULONGLONG minutes = seconds / 60;
  seconds %= 60;
  wchar_t text[128]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::Uptime), days, hours, minutes, seconds);
  SetWindowTextW(g.uptime, text);
}

void UpdateLidCounter() {
  wchar_t text[128]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::LidCounter), g.lidCloseCount);
  SetWindowTextW(g.lidCounter, text);
}

void UpdateControls() {
  SendMessageW(g.enabled, BM_SETCHECK, g.lidActionEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.acOff, BM_SETCHECK, g.acAction == kPowerNone ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.acSleep, BM_SETCHECK, g.acAction == kPowerSleep ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.acHibernate, BM_SETCHECK, g.acAction == kPowerHibernate ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.dcOff, BM_SETCHECK, g.dcAction == kPowerNone ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.dcSleep, BM_SETCHECK, g.dcAction == kPowerSleep ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.dcHibernate, BM_SETCHECK, g.dcAction == kPowerHibernate ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.lock, BM_SETCHECK, g.lockOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.killProcs, BM_SETCHECK, g.killProcessesOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.autostartBox, BM_SETCHECK, g.autostartEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.startMinimized, BM_SETCHECK, g.startMinimizedEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.notify, BM_SETCHECK, g.notifyEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.restore, BM_SETCHECK, g.restoreOnExit ? BST_CHECKED : BST_UNCHECKED, 0);
  SetWindowTextW(g.enabled, Text(TextId::LidActionEnabled));
  SetWindowTextW(g.lock, Text(TextId::LockOnClose));
  SetWindowTextW(g.killProcs, Text(TextId::KillProcessesOnClose));
  SetWindowTextW(g.autostartBox, Text(TextId::Autostart));
  SetWindowTextW(g.startMinimized, Text(TextId::StartMinimized));
  SetWindowTextW(g.notify, Text(TextId::Notify));
  SetWindowTextW(g.restore, Text(TextId::Restore));
  UpdateLidCounter();
  UpdateCurrentAction();
  UpdateTray();
}

void ToggleEnabled() {
  g.lidActionEnabled = !g.lidActionEnabled;
  SaveSettings();
  ApplyPowerSettings();
  UpdateControls();
  ShowNotification(g.lidActionEnabled ? Text(TextId::NotifyOn) : Text(TextId::NotifyOff));
  Log(g.lidActionEnabled ? Text(TextId::NotifyOn) : Text(TextId::NotifyOff));
}

void KillConfiguredProcesses() {
  GetWindowTextW(g.processList, g.processListText, ARRAYSIZE(g.processListText));
  SaveSettings();
  wchar_t list[512]{};
  lstrcpynW(list, g.processListText, ARRAYSIZE(list));
  wchar_t token[128]{};
  DWORD pos = 0;
  for (DWORD i = 0;; ++i) {
    wchar_t ch = list[i];
    bool sep = ch == 0 || ch == L',' || ch == L';' || ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
    if (!sep && pos + 1 < ARRAYSIZE(token)) {
      token[pos++] = ch;
    }
    if (sep && pos > 0) {
      token[pos] = 0;
      wchar_t cmd[768]{};
      std::swprintf(cmd, ARRAYSIZE(cmd), L"taskkill /IM \"%s\" /F /T", token);
      RunHidden(cmd);
      pos = 0;
    }
    if (ch == 0) break;
  }
}

void OnLidClosed() {
  ++g.lidCloseCount;
  UpdateLidCounter();
  Log(g.polish ? L"Wykryto zamknięcie klapy" : L"Lid close detected");
  if (g.lockOnClose) {
    Log(g.polish ? L"Blokuję ekran" : L"Locking screen");
    LockWorkStation();
  }
  if (g.killProcessesOnClose) {
    Log(g.polish ? L"Zabijam skonfigurowane procesy" : L"Killing configured processes");
    KillConfiguredProcesses();
  }
}

void ShowTrayMenu() {
  POINT pt{};
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_TOGGLE, g.lidActionEnabled ? Text(TextId::DisableLidSleep) : Text(TextId::EnableLidSleep));
  AppendMenuW(menu, MF_STRING, IDM_SHOW, Text(TextId::ShowWindow));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_EXIT, Text(TextId::Exit));
  SetForegroundWindow(g.hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.hwnd, nullptr);
  DestroyMenu(menu);
}

HWND AddButton(const wchar_t* text, int id, int x, int y, int w, int h, DWORD style) {
  return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                         g.hwnd, reinterpret_cast<HMENU>(id), GetModuleHandleW(nullptr), nullptr);
}

HWND AddStatic(const wchar_t* text, int id, int x, int y, int w, int h) {
  return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h,
                         g.hwnd, reinterpret_cast<HMENU>(id), GetModuleHandleW(nullptr), nullptr);
}

void CreateControls() {
  g.enabled = AddButton(Text(TextId::LidActionEnabled), IDC_ENABLED, 16, 12, 380, 22, BS_AUTOCHECKBOX);

  AddStatic(Text(TextId::AcLabel), -1, 16, 44, 80, 20);
  g.acOff = AddButton(Text(TextId::Off), IDC_AC_OFF, 96, 42, 70, 22, BS_AUTORADIOBUTTON | WS_GROUP);
  g.acSleep = AddButton(Text(TextId::Sleep), IDC_AC_SLEEP, 170, 42, 80, 22, BS_AUTORADIOBUTTON);
  g.acHibernate = AddButton(Text(TextId::Hibernate), IDC_AC_HIBERNATE, 255, 42, 100, 22, BS_AUTORADIOBUTTON);

  AddStatic(Text(TextId::DcLabel), -1, 16, 72, 80, 20);
  g.dcOff = AddButton(Text(TextId::Off), IDC_DC_OFF, 96, 70, 70, 22, BS_AUTORADIOBUTTON | WS_GROUP);
  g.dcSleep = AddButton(Text(TextId::Sleep), IDC_DC_SLEEP, 170, 70, 80, 22, BS_AUTORADIOBUTTON);
  g.dcHibernate = AddButton(Text(TextId::Hibernate), IDC_DC_HIBERNATE, 255, 70, 100, 22, BS_AUTORADIOBUTTON);

  g.lock = AddButton(Text(TextId::LockOnClose), IDC_LOCK, 16, 104, 330, 22, BS_AUTOCHECKBOX);
  g.killProcs = AddButton(Text(TextId::KillProcessesOnClose), IDC_KILL_PROCS, 16, 130, 330, 22, BS_AUTOCHECKBOX);
  AddStatic(Text(TextId::ProcessList), -1, 16, 158, 130, 20);
  g.processList = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g.processListText,
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 154, 270, 24,
                                  g.hwnd, reinterpret_cast<HMENU>(IDC_PROCESS_LIST), GetModuleHandleW(nullptr), nullptr);

  g.autostartBox = AddButton(Text(TextId::Autostart), IDC_AUTOSTART, 16, 190, 220, 22, BS_AUTOCHECKBOX);
  g.startMinimized = AddButton(Text(TextId::StartMinimized), IDC_START_MIN, 240, 190, 230, 22, BS_AUTOCHECKBOX);
  g.notify = AddButton(Text(TextId::Notify), IDC_NOTIFY, 16, 216, 300, 22, BS_AUTOCHECKBOX);
  g.restore = AddButton(Text(TextId::Restore), IDC_RESTORE, 16, 242, 360, 22, BS_AUTOCHECKBOX);

  g.current = AddStatic(L"", IDC_CURRENT, 16, 276, 430, 20);
  g.sleepCounter = AddStatic(L"", IDC_SLEEP_COUNTER, 16, 300, 430, 20);
  g.lidCounter = AddStatic(L"", IDC_LID_COUNTER, 16, 324, 430, 20);
  g.uptime = AddStatic(L"", IDC_UPTIME, 16, 348, 430, 20);
  AddStatic(Text(TextId::LogStart), -1, 16, 378, 130, 20);
  g.log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                          16, 400, 454, 130, g.hwnd, reinterpret_cast<HMENU>(IDC_LOG), GetModuleHandleW(nullptr), nullptr);

  UpdateControls();
  UpdateUptime();
  UpdateSleepCount();
  Log(g.polish ? L"Aplikacja uruchomiona" : L"Application started");
}

void SaveProcessListFromUi() {
  if (!g.processList) return;
  GetWindowTextW(g.processList, g.processListText, ARRAYSIZE(g.processListText));
  SaveSettings();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      g.hwnd = hwnd;
      CaptureOriginalActions();
      g.iconOn = MakeIcon(RGB(0, 180, 60));
      g.iconOff = MakeIcon(RGB(210, 40, 40));
      CreateControls();
      AddTray();
      SetTimer(hwnd, IDT_UPTIME, 1000, nullptr);
      RegisterHotKey(hwnd, HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT, 'L');
      g.lidNotify = RegisterPowerSettingNotification(hwnd, &kGuidLidSwitchStateChange, DEVICE_NOTIFY_WINDOW_HANDLE);
      ApplyPowerSettings();
      if (g.startHidden) ShowWindow(hwnd, SW_HIDE);
      return 0;

    case WM_TIMER:
      if (wp == IDT_UPTIME) {
        UpdateUptime();
        if (++g.uptimeTicks >= 60) {
          g.uptimeTicks = 0;
          UpdateSleepCount();
          UpdateCurrentAction();
        }
        return 0;
      }
      break;

    case WM_HOTKEY:
      if (wp == HOTKEY_TOGGLE) {
        ToggleEnabled();
        return 0;
      }
      break;

    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case IDM_TOGGLE:
        case IDC_ENABLED:
          ToggleEnabled();
          return 0;
        case IDC_AC_OFF:
        case IDC_AC_SLEEP:
        case IDC_AC_HIBERNATE:
          g.acAction = LOWORD(wp) == IDC_AC_OFF ? kPowerNone : (LOWORD(wp) == IDC_AC_SLEEP ? kPowerSleep : kPowerHibernate);
          SaveSettings();
          ApplyPowerSettings();
          UpdateControls();
          Log(g.polish ? L"Zmieniono akcję AC" : L"Changed AC action");
          return 0;
        case IDC_DC_OFF:
        case IDC_DC_SLEEP:
        case IDC_DC_HIBERNATE:
          g.dcAction = LOWORD(wp) == IDC_DC_OFF ? kPowerNone : (LOWORD(wp) == IDC_DC_SLEEP ? kPowerSleep : kPowerHibernate);
          SaveSettings();
          ApplyPowerSettings();
          UpdateControls();
          Log(g.polish ? L"Zmieniono akcję baterii" : L"Changed battery action");
          return 0;
        case IDC_LOCK:
          g.lockOnClose = SendMessageW(g.lock, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          return 0;
        case IDC_KILL_PROCS:
          g.killProcessesOnClose = SendMessageW(g.killProcs, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveProcessListFromUi();
          return 0;
        case IDC_AUTOSTART:
          SetAutostart(SendMessageW(g.autostartBox, BM_GETCHECK, 0, 0) == BST_CHECKED);
          UpdateControls();
          return 0;
        case IDC_START_MIN:
          g.startMinimizedEnabled = SendMessageW(g.startMinimized, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          if (g.autostartEnabled) SetAutostart(true);
          return 0;
        case IDC_NOTIFY:
          g.notifyEnabled = SendMessageW(g.notify, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          return 0;
        case IDC_RESTORE:
          g.restoreOnExit = SendMessageW(g.restore, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          return 0;
        case IDC_PROCESS_LIST:
          if (HIWORD(wp) == EN_KILLFOCUS) SaveProcessListFromUi();
          return 0;
        case IDM_SHOW:
          ShowWindow(hwnd, SW_SHOWNORMAL);
          SetForegroundWindow(hwnd);
          return 0;
        case IDM_EXIT:
          DestroyWindow(hwnd);
          return 0;
      }
      break;

    case WM_TRAY:
      if (LOWORD(lp) == WM_LBUTTONUP) ToggleEnabled();
      if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) ShowTrayMenu();
      if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
      }
      return 0;

    case WM_POWERBROADCAST:
      if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        UpdateSleepCount();
        UpdateCurrentAction();
        return TRUE;
      }
      if (wp == PBT_POWERSETTINGCHANGE) {
        auto* setting = reinterpret_cast<POWERBROADCAST_SETTING*>(lp);
        if (setting && IsEqualGUID(setting->PowerSetting, kGuidLidSwitchStateChange) && setting->DataLength >= sizeof(DWORD)) {
          DWORD lidOpen = *reinterpret_cast<DWORD*>(setting->Data);
          if (lidOpen == 0) OnLidClosed();
        }
      }
      return TRUE;

    case WM_CLOSE:
      ShowWindow(hwnd, SW_HIDE);
      return 0;

    case WM_DESTROY:
      SaveProcessListFromUi();
      RestoreOriginalSettings();
      KillTimer(hwnd, IDT_UPTIME);
      UnregisterHotKey(hwnd, HOTKEY_TOGGLE);
      if (g.lidNotify) UnregisterPowerSettingNotification(g.lidNotify);
      RemoveTray();
      if (g.iconOn) DestroyIcon(g.iconOn);
      if (g.iconOff) DestroyIcon(g.iconOff);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int show) {
  LoadSettings();
  g.startHidden = cmdLine && std::wcsstr(cmdLine, L"--minimized");

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.lpszClassName = kClassName;
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, kClassName, kAppName, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 500, 585, nullptr, nullptr, instance, nullptr);
  if (!hwnd) return 1;

  ShowWindow(hwnd, g.startHidden ? SW_HIDE : show);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
