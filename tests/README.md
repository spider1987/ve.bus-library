# Prüfungen

Vom Projektverzeichnis aus ausführen:

```powershell
python tests/check_web_utf8.py
node tests/web_ui_syntax.cjs
node tests/login_ui.cjs
node tests/auto_controls_ui.cjs
node tests/auto_settings_ui.cjs
node tests/energy_ui.cjs
node tests/dashboard_ui.cjs
node tests/theme_ui.cjs
node tests/info_ui.cjs
```

Die UI-Prüfungen verwenden simulierte API-Antworten und senden keine
Gerätebefehle. Sie ersetzen keinen Test am angeschlossenen MultiPlus.

`energy_model_compile.cpp` prüft Integration und Tagesarchiv per
`static_assert`. Mit dem PlatformIO-Compiler und `-std=c++14 -c` übersetzen;
die Objektdatei gehört nach `.pio/build/`.

Die Compile-Time-Prüfungen in `AutoPower.h` und `WiFiRecovery.h` laufen bei jedem Firmware-Build.


Optionale Browserprüfung (Node.js 24 und Chrome):

```powershell
node tests/browser_oled_ui.cjs
```

Sie prüft alle drei Designs, gespeicherte Auswahl, volle Seitenbreite und alle
Einstellungsbereiche einschließlich der separaten Infoseite bei 1920, 1366, 760, 390 und 320 Pixeln. Sämtliche API-Antworten stammen von einem lokalen
Testserver. Screenshots mit simulierten Werten liegen in `.pio/build/ui-preview/`;
das temporäre Browserprofil wird nach dem Test entfernt. Ein abweichender
Chrome-Pfad kann über `CHROME_PATH` gesetzt werden.

`node tests/auto_priority_settings_ui.cjs` prüft BYD-Ladeziel, Wiederanlaufwerte, die erforderliche Abschaltspannung und die Anzeige gespeicherter Einstellungen. AutoPower.h enthält C++11-kompatible Compile-Time-Prüfungen für SoC-/Solar-Freigaben und durchgehende Überschusszeit einschließlich Datenlücken und millis-Überlauf. Physischer OFF/ON-Zyklus wurde damit nicht getestet.
