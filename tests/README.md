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

Native evcc-Regression (Ubuntu/WSL, vorhandener g++):
```
g++ -std=c++11 -I.pio/libdeps/mqtt_ha_control_test/ArduinoJson/src tests/evcc_reader.cpp -o .pio/build/evcc-reader-test
.pio/build/evcc-reader-test
```
Optionale Argumente sind lokale evcc-JSON-Dateien; es werden nur Messwerte
für den Test gelesen und keine Steuerbefehle gesendet.

Lastkorrektur: `node tests/load_correction_ui.cjs` prüft Eingabegrenzen und Anzeige. `g++ -std=c++11 tests/load_correction.cpp -o .pio/build/load-correction-test` und `.pio/build/load-correction-test` prüfen die produktive Spannungslogik ohne Gerät.

MQTT/Boost:
- `node tests/evcc_source_boost_ui.cjs`: Datenquelle, eigener Boost-SoC,
  Aktivierung, Persistenzanzeige und Eingabegrenzen.
- `g++ -std=c++11 -I.pio/libdeps/mqtt_ha_control_test/ArduinoJson/src tests/evcc_mqtt.cpp -o .pio/build/evcc-mqtt-test`
  und `.pio/build/evcc-mqtt-test`: produktiver Cache, Retain-Start, zusammenpassende
  Live-Daten, Zeitstempel, Datenlücken, Offline-Fall, Ladepunktlimit und Boost-Grenze.
  Optionaler Dateiparameter: aufgezeichnete Liste mit MQTT-`topic`, `payload`, `t`.
- `tests/evcc_reader.cpp` prüft zusätzlich echte HTTP-Boost-Felder und
  Ablehnung von Boost im ungeeigneten Lade-/Batteriemodus.

`node tests/auto_save_feedback.cjs` reproduziert die Kombination 47 V / Faktor 0,00005 / Untergrenze 0, prüft Fehlerhinweis und Eingabe-Erhalt, erfolgreichen Wiederholversuch, Polling während eines laufenden Speicherns, Doppelklick-Sperre und Fehler-/Nicht-JSON-Antworten.

24-h-Verlauf/Flash-Datenformat:
```
g++ -std=c++11 tests/history48.cpp -o .pio/build/history48-test
.pio/build/history48-test
node tests/history_ui.cjs
```
Geprüft werden der produktive 24-h-Ring, Neustart-Dateireplay mit Zeitlücken,
CRC/unterbrochene Datensätze, abgelaufene/zukünftige Zeitpunkte, Mittelwerte
mit ungültigem SoC und die echte UTC-Zeitachse. Keine dieser Prüfungen ersetzt
LittleFS-Mount, Schreiben/Lesen und Neustart auf dem tatsächlichen LILYGO.

`g++ -std=c++11 tests/ess_restart.cpp -o .pio/build/ess-restart-test` und
`.pio/build/ess-restart-test`: 120-s-Fehlererkennung, OFF-Bestätigung,
10-s-Aus-Pause, ON-Bestätigung mit neuen Messwerten, fehlende Daten,
Zeitüberschreitung, manueller Abbruch.

`g++ -std=c++11 tests/restart_budget.cpp -o .pio/build/restart-budget-test` und
`.pio/build/restart-budget-test`: zwei Versuche am Tag, 30 Minuten Abstand,
Wiederherstellung des gespeicherten Records, Mitternacht ohne Umgehen des
Abstands, zurückspringende/ungültige Uhrzeit und fehlerhafte Records.
Die tatsächliche NVS-Speicherung muss nach Installation am Gerät geprüft werden.
