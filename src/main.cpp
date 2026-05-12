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

constexpr int IDM_TOGGLE = 100;
constexpr int IDM_SHOW = 101;
constexpr int IDM_EXIT = 102;
constexpr int IDC_ENABLED = 200;
constexpr int IDC_SLEEP = 201;
constexpr int IDC_HIBERNATE = 202;
constexpr int IDC_LOCK = 203;
constexpr int IDC_KILL_CHROME = 204;
constexpr int IDC_AUTOSTART = 205;
constexpr int IDC_COUNTER = 206;
constexpr int IDC_UPTIME = 207;
constexpr UINT_PTR IDT_UPTIME = 1;

// 0 = do nothing, 1 = sleep, 2 = hibernate.
constexpr DWORD kPowerNone = 0;
constexpr DWORD kPowerSleep = 1;
constexpr DWORD kPowerHibernate = 2;
constexpr GUID kGuidLidSwitchStateChange{
    0xBA3E0F4D, 0xB817, 0x4094, {0xA2, 0xD1, 0xD5, 0x63, 0x79, 0xE6, 0xA0, 0xF3}};

struct State {
  HWND hwnd{};
  HWND enabled{};
  HWND sleep{};
  HWND hibernate{};
  HWND lock{};
  HWND killChrome{};
  HWND autostartBox{};
  HWND counter{};
  HWND uptime{};
  HICON iconOn{};
  HICON iconOff{};
  HPOWERNOTIFY lidNotify{};
  bool lidActionEnabled = true;
  DWORD lidAction = kPowerSleep;
  bool lockOnClose = false;
  bool killChromeOnClose = false;
  bool autostartEnabled = false;
  DWORD sleepCount = 0;
  DWORD uptimeTicks = 0;
  bool polish = false;
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
  SleepOnClose,
  HibernateOnClose,
  LockOnClose,
  KillChromeOnClose,
  Autostart,
  SleepCounter,
  Uptime,
};

const wchar_t* Text(TextId id) {
  if (g.polish) {
    switch (id) {
      case TextId::TraySleepOn: return L"usypianie włączone";
      case TextId::TraySleepOff: return L"usypianie wyłączone";
      case TextId::DisableLidSleep: return L"Wyłącz usypianie po klapie";
      case TextId::EnableLidSleep: return L"Włącz usypianie po klapie";
      case TextId::ShowWindow: return L"Pokaż okno";
      case TextId::Exit: return L"Zamknij";
      case TextId::LidActionEnabled: return L"Usypianie/hibernacja po zamknięciu klapy włączone";
      case TextId::SleepOnClose: return L"Uśpij po zamknięciu";
      case TextId::HibernateOnClose: return L"Hibernuj po zamknięciu";
      case TextId::LockOnClose: return L"Zablokuj ekran po zamknięciu klapy";
      case TextId::KillChromeOnClose: return L"Zabij chrome.exe po zamknięciu klapy";
      case TextId::Autostart: return L"Autostart z systemem";
      case TextId::SleepCounter: return L"Uśpienia/hibernacje od startu systemu: %lu";
      case TextId::Uptime: return L"Uptime systemu: %llud %02llu:%02llu:%02llu";
    }
  }

  switch (id) {
    case TextId::TraySleepOn: return L"sleep enabled";
    case TextId::TraySleepOff: return L"sleep disabled";
    case TextId::DisableLidSleep: return L"Disable lid sleep";
    case TextId::EnableLidSleep: return L"Enable lid sleep";
    case TextId::ShowWindow: return L"Show window";
    case TextId::Exit: return L"Exit";
    case TextId::LidActionEnabled: return L"Sleep/hibernate on lid close enabled";
    case TextId::SleepOnClose: return L"Sleep on lid close";
    case TextId::HibernateOnClose: return L"Hibernate on lid close";
    case TextId::LockOnClose: return L"Lock screen on lid close";
    case TextId::KillChromeOnClose: return L"Kill chrome.exe on lid close";
    case TextId::Autostart: return L"Start with Windows";
    case TextId::SleepCounter: return L"Sleeps/hibernations since system boot: %lu";
    case TextId::Uptime: return L"System uptime: %llud %02llu:%02llu:%02llu";
  }
  return L"";
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

void LoadSettings() {
  g.polish = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_POLISH;
  g.lidActionEnabled = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"LidActionEnabled", 1) != 0;
  g.lidAction = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"LidAction", kPowerSleep);
  if (g.lidAction != kPowerSleep && g.lidAction != kPowerHibernate) g.lidAction = kPowerSleep;
  g.lockOnClose = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"LockOnClose", 0) != 0;
  g.killChromeOnClose = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"KillChromeOnClose", 0) != 0;

  HKEY key{};
  g.autostartEnabled = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) == ERROR_SUCCESS;
  if (g.autostartEnabled) {
    g.autostartEnabled = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);
  }
}

void SaveSettings() {
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"LidActionEnabled", g.lidActionEnabled ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"LidAction", g.lidAction);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"LockOnClose", g.lockOnClose ? 1 : 0);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"KillChromeOnClose", g.killChromeOnClose ? 1 : 0);
}

HICON MakeIcon(COLORREF color) {
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  HBITMAP colorBmp = CreateCompatibleBitmap(screen, 16, 16);
  HBITMAP maskBmp = CreateBitmap(16, 16, 1, 1, nullptr);
  HGDIOBJ old = SelectObject(dc, colorBmp);

  HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
  RECT rect{0, 0, 16, 16};
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
  wchar_t buffer[512]{};
  lstrcpynW(buffer, command, ARRAYSIZE(buffer));
  if (CreateProcessW(nullptr, buffer, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
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

  wchar_t buffer[768]{};
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

void ApplyPowerSettings() {
  DWORD action = g.lidActionEnabled ? g.lidAction : kPowerNone;
  wchar_t cmd[256]{};
  std::swprintf(cmd, ARRAYSIZE(cmd), L"powercfg /setacvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION %lu", action);
  RunHidden(cmd);
  std::swprintf(cmd, ARRAYSIZE(cmd), L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION %lu", action);
  RunHidden(cmd);
  RunHidden(L"powercfg /setactive SCHEME_CURRENT");
}

void SetAutostart(bool enabled) {
  HKEY key{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    return;
  }
  if (enabled) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    wchar_t quoted[MAX_PATH + 2]{};
    std::swprintf(quoted, ARRAYSIZE(quoted), L"\"%s\"", path);
    RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted),
                   static_cast<DWORD>((lstrlenW(quoted) + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, kRunValue);
  }
  RegCloseKey(key);
  g.autostartEnabled = enabled;
}

void UpdateControls() {
  SendMessageW(g.enabled, BM_SETCHECK, g.lidActionEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.sleep, BM_SETCHECK, g.lidAction == kPowerSleep ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.hibernate, BM_SETCHECK, g.lidAction == kPowerHibernate ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.lock, BM_SETCHECK, g.lockOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.killChrome, BM_SETCHECK, g.killChromeOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.autostartBox, BM_SETCHECK, g.autostartEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
  wchar_t text[128]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::SleepCounter), g.sleepCount);
  SetWindowTextW(g.counter, text);
  SetWindowTextW(g.enabled, Text(TextId::LidActionEnabled));
  SetWindowTextW(g.sleep, Text(TextId::SleepOnClose));
  SetWindowTextW(g.hibernate, Text(TextId::HibernateOnClose));
  SetWindowTextW(g.lock, Text(TextId::LockOnClose));
  SetWindowTextW(g.killChrome, Text(TextId::KillChromeOnClose));
  SetWindowTextW(g.autostartBox, Text(TextId::Autostart));
  UpdateTray();
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
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::Uptime),
                days, hours, minutes, seconds);
  SetWindowTextW(g.uptime, text);
}

void UpdateSleepCount() {
  g.sleepCount = QuerySleepCountSinceBoot();
  wchar_t text[128]{};
  std::swprintf(text, ARRAYSIZE(text), Text(TextId::SleepCounter), g.sleepCount);
  SetWindowTextW(g.counter, text);
}

void ToggleEnabled() {
  g.lidActionEnabled = !g.lidActionEnabled;
  SaveSettings();
  ApplyPowerSettings();
  UpdateControls();
}

void OnLidClosed() {
  if (g.lockOnClose) LockWorkStation();
  if (g.killChromeOnClose) RunHidden(L"taskkill /IM chrome.exe /F /T");
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

void CreateControls() {
  g.enabled = AddButton(Text(TextId::LidActionEnabled), IDC_ENABLED, 16, 16, 390, 24, BS_AUTOCHECKBOX);
  g.sleep = AddButton(Text(TextId::SleepOnClose), IDC_SLEEP, 32, 52, 180, 24, BS_AUTORADIOBUTTON | WS_GROUP);
  g.hibernate = AddButton(Text(TextId::HibernateOnClose), IDC_HIBERNATE, 220, 52, 190, 24, BS_AUTORADIOBUTTON);
  g.lock = AddButton(Text(TextId::LockOnClose), IDC_LOCK, 16, 92, 300, 24, BS_AUTOCHECKBOX);
  g.killChrome = AddButton(Text(TextId::KillChromeOnClose), IDC_KILL_CHROME, 16, 124, 320, 24, BS_AUTOCHECKBOX);
  g.autostartBox = AddButton(Text(TextId::Autostart), IDC_AUTOSTART, 16, 156, 220, 24, BS_AUTOCHECKBOX);
  g.counter = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 198, 360, 24,
                              g.hwnd, reinterpret_cast<HMENU>(IDC_COUNTER), GetModuleHandleW(nullptr), nullptr);
  g.uptime = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 226, 360, 24,
                             g.hwnd, reinterpret_cast<HMENU>(IDC_UPTIME), GetModuleHandleW(nullptr), nullptr);
  UpdateControls();
  UpdateUptime();
  UpdateSleepCount();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      g.hwnd = hwnd;
      g.iconOn = MakeIcon(RGB(0, 180, 60));
      g.iconOff = MakeIcon(RGB(210, 40, 40));
      CreateControls();
      AddTray();
      SetTimer(hwnd, IDT_UPTIME, 1000, nullptr);
      g.lidNotify = RegisterPowerSettingNotification(hwnd, &kGuidLidSwitchStateChange, DEVICE_NOTIFY_WINDOW_HANDLE);
      ApplyPowerSettings();
      return 0;

    case WM_TIMER:
      if (wp == IDT_UPTIME) {
        UpdateUptime();
        if (++g.uptimeTicks >= 60) {
          g.uptimeTicks = 0;
          UpdateSleepCount();
        }
        return 0;
      }
      break;

    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case IDM_TOGGLE:
        case IDC_ENABLED:
          ToggleEnabled();
          return 0;
        case IDC_SLEEP:
          g.lidAction = kPowerSleep;
          SaveSettings();
          ApplyPowerSettings();
          UpdateControls();
          return 0;
        case IDC_HIBERNATE:
          g.lidAction = kPowerHibernate;
          SaveSettings();
          ApplyPowerSettings();
          UpdateControls();
          return 0;
        case IDC_LOCK:
          g.lockOnClose = SendMessageW(g.lock, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          return 0;
        case IDC_KILL_CHROME:
          g.killChromeOnClose = SendMessageW(g.killChrome, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SaveSettings();
          return 0;
        case IDC_AUTOSTART:
          SetAutostart(SendMessageW(g.autostartBox, BM_GETCHECK, 0, 0) == BST_CHECKED);
          UpdateControls();
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
      if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) ShowTrayMenu();
      if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
      }
      return 0;

    case WM_POWERBROADCAST:
      if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        UpdateSleepCount();
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
      KillTimer(hwnd, IDT_UPTIME);
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  LoadSettings();

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.lpszClassName = kClassName;
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, kClassName, kAppName, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 440, 290, nullptr, nullptr, instance, nullptr);
  if (!hwnd) return 1;

  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
