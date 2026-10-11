// Passive VE.Bus capture: no protocol library, no UART TX pin, no commands.
#include <Arduino.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer web(80);
struct Capture { uint32_t ms; uint8_t len; uint8_t data[64]; };
static Capture captures[512];
static uint16_t head = 0, rows = 0;
static uint32_t lastRx = 0;
static const char page[] PROGMEM = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Passive VE.Bus-Diagnose</title><style>body{background:#101720;color:#e4edf8;font:16px system-ui;max-width:850px;margin:40px auto;padding:20px}section{background:#18222e;padding:24px;border-radius:14px}a{color:#79b7ff}pre{white-space:pre-wrap;overflow-wrap:anywhere}</style><section><h1>Passive VE.Bus-Diagnose</h1><p>Nur Empfang bei 256000 Baud. Keine WR-Steuerbefehle.</p><pre id="status">Warte auf Status …</pre><p><a href="/capture.txt">Empfangslog herunterladen</a></p><p>Log im RAM: letzte 512 Empfangsblöcke. Bleibt ohne geöffneten Browser erhalten; Neustart löscht ihn. Empfang allein bestätigt noch keine gültigen Telegramme.</p></section><script>async function poll(){try{let r=await fetch('/api/diag',{cache:'no-store'});if(!r.ok)throw Error(r.status);let d=await r.json();document.getElementById('status').textContent='Empfangen: '+d.rx_bytes+' Bytes\nGespeicherte Blöcke: '+d.rows+'\nLaufzeit: '+Math.floor(d.uptime_ms/1000)+' s\nLetzter Empfang: '+(d.last_rx_ms===null?'noch keiner':d.last_rx_ms+' ms seit Start')+'\nTX-Pegel: '+d.tx_level+' (fest HIGH)\nWLAN: '+d.rssi+' dBm'}catch(e){document.getElementById('status').textContent='Diagnose nicht erreichbar: '+e.message}}poll();setInterval(poll,2000)</script></html>)HTML";

static uint32_t received = 0;
static uint32_t lastReport = 0;

void setup()
{
    // MAX13487E auto-direction: hold DI at idle HIGH; never route UART TX to it.
    pinMode(19, OUTPUT);
    digitalWrite(19, LOW);
    pinMode(22, OUTPUT);
    digitalWrite(22, HIGH);
    pinMode(17, OUTPUT);
    digitalWrite(17, LOW); // /RE: receiver enabled
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH); // transceiver supply

    Serial.begin(115200);
    uart_config_t config = {};
    config.baud_rate = 256000;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, 21,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 4096, 0, 0, nullptr, 0));
    digitalWrite(19, HIGH);
    delay(500);
    Serial.println("PASSIVE VE.Bus RX: 256000 baud; GPIO22 fixed HIGH; no UART TX routing.");
    Serial.println("No switch or ESS commands. Waiting for bytes.");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(); // reuse credentials saved by the normal firmware
    web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", page); });
    web.on("/api/diag", [] {
        char json[320];
        char last[24];
        if (received) snprintf(last, sizeof(last), "%lu", (unsigned long)lastRx);
        else strcpy(last, "null");
        snprintf(json, sizeof(json), "{\"passive\":true,\"rx_bytes\":%lu,\"rows\":%u,\"uptime_ms\":%lu,\"last_rx_ms\":%s,\"tx_level\":%d,\"rssi\":%d}",
                 (unsigned long)received, rows, (unsigned long)millis(), last,
                 gpio_get_level(GPIO_NUM_22), WiFi.RSSI());
        web.sendHeader("Cache-Control", "no-store");
        web.send(200, "application/json", json);
    });
    web.on("/capture.txt", [] {
        web.sendHeader("Content-Disposition", "attachment; filename=vebus-passive.txt");
        web.sendHeader("Cache-Control", "no-store");
        web.setContentLength(CONTENT_LENGTH_UNKNOWN);
        web.send(200, "text/plain", "Passive capture, 256000 baud; ms since boot; HEX bytes\n");
        uint16_t first = (head + 512 - rows) % 512;
        for (uint16_t r = 0; r < rows; ++r) {
            const Capture &c = captures[(first + r) % 512];
            char line[220];
            int n = snprintf(line, sizeof(line), "%lu: ", (unsigned long)c.ms);
            for (uint8_t i = 0; i < c.len; ++i) n += snprintf(line+n, sizeof(line)-n, "%02X ", c.data[i]);
            line[n++] = '\n'; line[n] = 0;
            web.sendContent(line);
        }
        web.sendContent("");
    });
    web.begin();
}

void loop()
{
    uint8_t bytes[64];
    int count = uart_read_bytes(UART_NUM_1, bytes, sizeof(bytes), pdMS_TO_TICKS(20));
    if (count > 0) {
        received += count;
        lastRx = millis();
        captures[head].ms = lastRx;
        captures[head].len = count;
        memcpy(captures[head].data, bytes, count);
        head = (head + 1) % 512;
        if (rows < 512) ++rows;
        Serial.printf("RX %lu ms: ", (unsigned long)millis());
        for (int i = 0; i < count; ++i) Serial.printf("%02X ", bytes[i]);
        Serial.println();
    }
    web.handleClient();
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.printf("PASSIVE: received=%lu bytes; TX GPIO22=%d; /RE=%d; supply=%d; enable=%d\n",
                      (unsigned long)received, gpio_get_level(GPIO_NUM_22),
                      gpio_get_level(GPIO_NUM_17), gpio_get_level(GPIO_NUM_16),
                      gpio_get_level(GPIO_NUM_19));
    }
}
