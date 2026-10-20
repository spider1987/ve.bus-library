// Optional real-browser layout check. All APIs are local fixtures; no device is contacted.
const fs = require("node:fs");
const path = require("node:path");
const os = require("node:os");
const http = require("node:http");
const { spawn } = require("node:child_process");
const assert = require("node:assert/strict");
const source = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const extract = (name) =>
  source.match(
    new RegExp(name + '\\[\\].*?R"rawliteral\\(([\\s\\S]*?)\\)rawliteral";'),
  )[1];
const pages = {
  "/": extract("INDEX_HTML"),
  "/admin/": extract("ADMIN_HTML"),
  "/login": extract("LOGIN_HTML"),
  "/info/system": extract("INFO_HTML"),
};
const state = {
  read_only: false,
  sync: "ON",
  mqtt: true,
  rssi: -55,
  device_id: "vebus_multiplus",
  ip: "192.0.2.1",
  host: "vebus-multiplus",
  default_pw: false,
  bat_volt: 52.6,
  battery_fresh: true,
  bat_current: -17,
  dc_current: -17,
  soc: 57,
  temp: null,
  temp_available: false,
  output_power: 25,
  mains_power: 900,
  ess_power: 1000,
  ess_power_eff: 1000,
  device_state: "Charge",
  charge_sub_state: "Bulk",
  switch_state: "on",
  virtual_mode: "OFF",
  uptime: 7200,
  free_heap: 90000,
  reset_reason: "Power on",
  mains_voltage: 233,
  mains_freq: 50,
  inv_voltage: 233,
  inv_freq: 50,
};
const fixtures = {
  "/api/system": {
    hostname: "vebus-multiplus",
    uptime_s: 7200,
    reset_reason: "Power on",
    reset_code: 1,
    crash_stage: "-",
    vebus_firmware: "1.23",
    hardware: { chip_model: "ESP32", revision: 3, cores: 2, cpu_mhz: 240 },
    memory: {
      heap_free: 90000,
      heap_total: 240000,
      heap_min_free: 80000,
      heap_max_block: 70000,
      flash_total: 4194304,
      sketch_used: 1200000,
      sketch_capacity: 1966080,
      psram_total: 0,
      psram_free: 0,
    },
    firmware: {
      application: "VE.Bus MQTT / LILYGO",
      build: "Fixture",
      sdk: "Fixture SDK",
      partition: "app0",
      read_only: false,
    },
    network: {
      connected: true,
      ip: "192.0.2.1",
      gateway: "192.0.2.254",
      subnet: "255.255.255.0",
      rssi: -55,
    },
    connections: {
      mqtt: true,
      vebus: true,
      wifi_reconnects: 0,
      mqtt_reconnects: 1,
    },
  },
  "/api/state": state,
  "/api/auto": {
    enabled: true,
    fresh: true,
    status: "Entladen",
    share_pct: 90,
    target_w: 1000,
    setpoint_w: 1000,
    demand_w: 1100,
    byd_w: 200,
    victron_w: 900,
    charge_limit_w: 3000,
    discharge_limit_w: 3000,
    cutoff_v: 47,
    resume_delta_v: 1,
    discharge_blocked: false,
    byd_threshold_w: 100,
    data_timeout_s: 35,
  },
  "/api/energy": {
    date: "2026-10-20",
    time_valid: true,
    fresh: true,
    storage_ok: true,
    today_charge_kwh: 3,
    today_discharge_kwh: 2.4,
    total_charge_kwh: 30,
    total_discharge_kwh: 24,
    today_measured_s: 3600,
    unassigned_charge_kwh: 0,
    unassigned_discharge_kwh: 0,
    mqtt_topic: "vebus/multiplus/energy",
    days: [
      {
        date: "2026-10-20",
        charged_kwh: 3,
        discharged_kwh: 2.4,
        measured_s: 3600,
      },
    ],
  },
  "/admin/api/config": {
    host: "192.0.2.2",
    port: 1883,
    user: "",
    device: "vebus_multiplus",
    prefix: "vebus/multiplus",
    ess_timeout: 0,
    nut_ups: "multiplus",
    nut_user: "",
    low_soc: 20,
    nominal_w: 4000,
    nut: true,
    apc: true,
    battery_wh: 0,
    pass_set: false,
    nut_pass_set: false,
  },
  "/api/ac-log": {
    running: false,
    started: false,
    fresh: true,
    battery_fresh: true,
    read_only: false,
    setpoint_w: 0,
    effective_w: 0,
    virtual_mode: false,
    power_w: 0,
    import_wh: 0,
    export_wh: 0,
    battery_charge_kwh: 0,
    battery_discharge_kwh: 0,
    battery_power_w: 0,
    elapsed_s: 0,
    measured_s: 0,
    rows: 0,
    rows_total: 0,
    battery_measured_s: 0,
  },
  "/api/history": {
    interval: 60,
    age: 0,
    data: Array.from({ length: 1440 }, (_, i) => [
      25,
      Math.round(700 + 300 * Math.sin(i / 70)),
      1000,
      Math.round(5200 + 100 * Math.sin(i / 200)),
      570,
    ]),
  },
};
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
(async () => {
  const server = http.createServer((req, res) => {
    const url = req.url.split("?")[0];
    res.setHeader("Cache-Control", "no-store");
    if (url === "/theme.js") {
      res.setHeader("Content-Type", "application/javascript");
      return res.end(extract("THEME_JS"));
    }
    if (url === "/style.css") {
      res.setHeader("Content-Type", "text/css");
      return res.end(extract("STYLE_CSS"));
    }
    if (pages[url]) {
      res.setHeader("Content-Type", "text/html; charset=utf-8");
      return res.end(pages[url]);
    }
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(fixtures[url] || {}));
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), "vebus-oled-qa-"));
  const chrome = spawn(
    process.env.CHROME_PATH ||
      "C:/Program Files/Google/Chrome/Application/chrome.exe",
    [
      "--headless",
      "--disable-gpu",
      "--no-sandbox",
      "--remote-debugging-port=0",
      "--user-data-dir=" + profile,
      "about:blank",
    ],
    { windowsHide: true, stdio: "ignore" },
  );
  let ws;
  try {
    let port;
    for (let n = 0; n < 100; n++) {
      const file = path.join(profile, "DevToolsActivePort");
      if (fs.existsSync(file)) {
        port = Number(fs.readFileSync(file, "utf8").split("\n")[0]);
        break;
      }
      await pause(100);
    }
    assert(port, "Chrome debugging endpoint did not start");
    const tabs = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
    ws = new WebSocket(
      tabs.find((tab) => tab.type === "page").webSocketDebuggerUrl,
    );
    await new Promise((resolve) =>
      ws.addEventListener("open", resolve, { once: true }),
    );
    let id = 0;
    const pending = new Map(),
      errors = [];
    ws.onmessage = (event) => {
      const result = JSON.parse(event.data);
      if (result.method === "Runtime.exceptionThrown")
        errors.push(result.params.exceptionDetails);
      if (pending.has(result.id)) {
        pending.get(result.id)(result);
        pending.delete(result.id);
      }
    };
    const call = (method, params = {}) =>
      new Promise((resolve) => {
        const next = ++id;
        pending.set(next, resolve);
        ws.send(JSON.stringify({ id: next, method, params }));
      });
    await call("Page.enable");
    await call("Runtime.enable");
    const out = ".pio/build/ui-preview";
    fs.mkdirSync(out, { recursive: true });
    for (const width of [1920, 1366, 760, 390, 320]) {
      await call("Emulation.setDeviceMetricsOverride", {
        width,
        height: 1080,
        deviceScaleFactor: 1,
        mobile: width === 390,
      });
      for (const route of [
        "/",
        "/admin/#start",
        "/admin/#automatik",
        "/admin/#manuell",
        "/admin/#test",
        "/admin/#statistik",
        "/admin/#anbindungen",
        "/admin/#zugang",
        "/admin/#system",
        "/login",
        "/info/system",
      ]) {
        errors.length = 0;
        await call("Page.navigate", {
          url: `http://127.0.0.1:${server.address().port}${route}`,
        });
        await pause(180);
        const response = await call("Runtime.evaluate", {
          expression: `JSON.stringify({width:innerWidth,clientWidth:document.documentElement.clientWidth,scroll:document.documentElement.scrollWidth,main:document.querySelector('main').getBoundingClientRect().width,bg:getComputedStyle(document.body).backgroundColor,tiles:document.querySelectorAll('.tile').length,blackTiles:[...document.querySelectorAll('.tile')].every(el=>getComputedStyle(el).backgroundColor==='rgb(0, 0, 0)'),cardBg:document.querySelector('.card')?getComputedStyle(document.querySelector('.card')).backgroundColor:null})`,
          returnByValue: true,
        });
        const v = JSON.parse(response.result.result.value);
        assert.equal(errors.length, 0, JSON.stringify(errors));
        assert(v.scroll <= v.width, route + " overflows: " + JSON.stringify(v));
        assert.equal(v.bg, "rgb(0, 0, 0)");
        if (route === "/login") assert(v.main <= 480);
        else
          assert.equal(
            v.main,
            v.clientWidth,
            "Page must fill available viewport width",
          );
        if (route === "/info/system") {
          const info = await call("Runtime.evaluate", {
            expression:
              "JSON.stringify({host:document.getElementById('infoHost').textContent,visible:!document.getElementById('infoData').hidden,rows:document.getElementById('hardwareRows').children.length,diag:document.querySelectorAll('.diagnostic-group').length})",
            returnByValue: true,
          });
          const data = JSON.parse(info.result.result.value);
          assert.equal(data.host, "vebus-multiplus");
          assert(data.visible);
          assert(data.rows >= 5);
          assert.equal(data.diag, 4);
          if (width === 1920 || width === 390) {
            const shot = await call("Page.captureScreenshot", {
              captureBeyondViewport: false,
            });
            fs.writeFileSync(
              path.join(out, "info-system-" + width + ".png"),
              Buffer.from(shot.result.data, "base64"),
            );
          }
        }
        if (route === "/") {
          assert.equal(v.tiles, 8);
          assert(v.blackTiles);
          assert.equal(v.cardBg, "rgb(0, 0, 0)");
        }
        if (
          (width === 1920 && route === "/") ||
          (width === 390 && route === "/") ||
          (width === 1920 && route === "/admin/#start")
        ) {
          const shot = await call("Page.captureScreenshot", {
            captureBeyondViewport: false,
          });
          fs.writeFileSync(
            path.join(
              out,
              (route === "/" ? "oled-dashboard-" : "oled-settings-") +
                width +
                ".png",
            ),
            Buffer.from(shot.result.data, "base64"),
          );
        }
      }
      await call("Page.navigate", {
        url: `http://127.0.0.1:${server.address().port}/`,
      });
      await pause(180);
      for (const [theme, bg] of [
        ["oled", "rgb(0, 0, 0)"],
        ["midnight", "rgb(16, 23, 32)"],
        ["graphite", "rgb(16, 16, 16)"],
      ]) {
        await call("Runtime.evaluate", {
          expression: `document.getElementById('themeSelect').value='${theme}';document.getElementById('themeSelect').dispatchEvent(new Event('change'))`,
        });
        const r = await call("Runtime.evaluate", {
          expression:
            "JSON.stringify({theme:document.documentElement.dataset.theme,saved:localStorage.getItem('vebus-theme'),bg:getComputedStyle(document.body).backgroundColor,scroll:document.documentElement.scrollWidth,width:innerWidth})",
          returnByValue: true,
        });
        const value = JSON.parse(r.result.result.value);
        assert.equal(value.theme, theme);
        assert.equal(value.saved, theme);
        assert.equal(value.bg, bg);
        assert(value.scroll <= value.width);
        if (width === 1920) {
          const shot = await call("Page.captureScreenshot", {
            captureBeyondViewport: false,
          });
          fs.writeFileSync(
            path.join(out, "theme-" + theme + ".png"),
            Buffer.from(shot.result.data, "base64"),
          );
        }
      }
      await call("Page.navigate", {
        url: `http://127.0.0.1:${server.address().port}/admin/#automatik`,
      });
      await pause(180);
      const persisted = await call("Runtime.evaluate", {
        expression: "document.getElementById('themeSelect').value",
        returnByValue: true,
      });
      assert.equal(persisted.result.result.value, "graphite");
      await call("Runtime.evaluate", {
        expression:
          "document.getElementById('autoChargeLimit').value='2345';document.getElementById('themeSelect').value='midnight';document.getElementById('themeSelect').dispatchEvent(new Event('change'))",
      });
      const retained = await call("Runtime.evaluate", {
        expression: "document.getElementById('autoChargeLimit').value",
        returnByValue: true,
      });
      assert.equal(retained.result.result.value, "2345");
      await call("Page.navigate", {
        url: `http://127.0.0.1:${server.address().port}/login`,
      });
      await pause(180);
      const loginTheme = await call("Runtime.evaluate", {
        expression: "document.getElementById('themeSelect').value",
        returnByValue: true,
      });
      assert.equal(loginTheme.result.result.value, "midnight");
      await call("Runtime.evaluate", {
        expression:
          "document.getElementById('themeSelect').value='oled';document.getElementById('themeSelect').dispatchEvent(new Event('change'))",
      });
      console.log(
        "All three themes, persistence, unsaved settings and layout OK:",
        width,
      );
    }
    await call("Browser.close");
  } finally {
    if (ws) ws.close();
    chrome.kill();
    server.close();
    await pause(400);
    const resolved = path.resolve(profile),
      temp = path.resolve(os.tmpdir()) + path.sep;
    assert(
      resolved.startsWith(temp) &&
        path.basename(resolved).startsWith("vebus-oled-qa-"),
    );
    fs.rmSync(resolved, {
      recursive: true,
      force: true,
      maxRetries: 5,
      retryDelay: 300,
    });
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
