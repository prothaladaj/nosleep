# NoSleep Lid

Minimalna aplikacja Win32/C++ do przełączania akcji po zamknięciu klapy laptopa.

Funkcje:

- ikona w trayu: zielona, gdy system ma usypiać/hibernować po zamknięciu klapy; czerwona, gdy akcja jest wyłączona,
- zwykłe okno widoczne na pasku zadań,
- licznik uśpień/hibernacji od startu systemu,
- licznik zamknięć klapy od startu aplikacji,
- autostart per użytkownik przez `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`,
- opcjonalny start zminimalizowany do tray,
- osobna konfiguracja akcji po zamknięciu klapy dla zasilania AC i baterii: brak, uśpij albo hibernuj,
- podgląd aktualnej systemowej akcji AC/bateria,
- opcjonalne przywrócenie poprzednich ustawień po zamknięciu aplikacji,
- opcjonalny lock screen po zamknięciu klapy,
- opcjonalne ubicie skonfigurowanych procesów po zamknięciu klapy,
- proste powiadomienia tray po zmianie stanu,
- szybkie przełączenie lewym kliknięciem ikony tray,
- globalny skrót `Ctrl+Alt+L`,
- prosty log zdarzeń w oknie aplikacji,
- polski interfejs na polskim Windows, angielski na pozostałych systemach.

## Build

MinGW-w64:

```bat
x86_64-w64-mingw32-g++ -std=c++17 -finput-charset=UTF-8 -Os -s -municode -mwindows -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX src/main.cpp -o nosleep.exe -luser32 -lshell32 -ladvapi32 -lgdi32 -lpowrprof
```

Visual Studio Developer Prompt:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Binarka: `build\Release\nosleep.exe`.

Uwaga: zmiana ustawień zasilania przez `powercfg` może wymagać uprawnień zależnie od polityk systemowych. Autostart jest ustawiany tylko dla bieżącego użytkownika.
