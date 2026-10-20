const fs = require("node:fs");
const vm = require("node:vm");
const assert = require("node:assert/strict");
const source = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const page = source.match(
  /INFO_HTML\[\].*?R"rawliteral\(([\s\S]*?)\)rawliteral";/,
)[1];
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
const logic = script.slice(0, script.indexOf("refreshInfo();setInterval"));
const fixture = {
  hostname: "test-device",
  uptime_s: 90061,
  reset_reason: "Power on",
  reset_code: 1,
  crash_stage: "-",
  hardware: { chip_model: "ESP32", revision: 3, cores: 2, cpu_mhz: 240 },
  firmware: {
    application: "VE.Bus",
    build: "Fixture",
    sdk: "Fixture SDK",
    read_only: true,
    partition: "app0",
  },
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
};
function element() {
  return {
    textContent: "",
    hidden: false,
    children: [],
    append(...items) {
      this.children.push(...items);
    },
    replaceChildren(...items) {
      this.children = items;
    },
    setAttribute() {},
  };
}
(async () => {
  for (const mode of ["valid", "missing", "offline"]) {
    const els = {};
    const context = {
      document: {
        getElementById: (id) => (els[id] ??= element()),
        createElement: element,
      },
      fetch: async (url) => {
        if (mode === "offline") throw Error("offline");
        return {
          ok: true,
          json: async () =>
            url === "/api/state"
              ? { sync: "OFF" }
              : mode === "valid"
                ? fixture
                : {},
        };
      },
    };
    vm.createContext(context);
    vm.runInContext(logic, context);
    await vm.runInContext("pollSystem()", context);
    if (mode === "valid") {
      assert.equal(els.infoHost.textContent, "test-device");
      assert.equal(els.infoUptime.textContent, "1 Tage 01:01:01");
      assert.equal(els.infoCpu.textContent, "240 MHz");
      assert.equal(els.infoStage.textContent, "Nicht aufgezeichnet");
      assert.equal(els.memoryRows.children.length, 2);
      assert.equal(
        els.firmwareRows.children[3].children[1].textContent,
        "Nur Lesen",
      );
      assert.equal(els.infoData.hidden, false);
    } else if (mode === "missing") {
      assert.equal(els.infoHost.textContent, "Unbekannt");
      assert.equal(els.infoCpu.textContent, "Unbekannt");
      assert.equal(
        els.firmwareRows.children[3].children[1].textContent,
        "Unbekannt",
      );
    } else {
      assert.equal(els.infoData.hidden, true);
      assert(els.infoStatus.textContent.includes("nicht erreichbar"));
    }
    await vm.runInContext("pollDiagnosis()", context);
    assert(
      els.diagnosisStatus.textContent.includes(
        mode === "offline" ? "nicht erreichbar" : "Keine VE.Bus-Verbindung",
      ),
    );
  }
  const dashboard = source.match(
    /INDEX_HTML\[\].*?R"rawliteral\(([\s\S]*?)\)rawliteral";/,
  )[1];
  assert(!dashboard.includes('id="diagnose"'));
  assert(!source.includes('href="/#diagnose"'));
  console.log(
    "Info page: real fields, unknown values, memory, offline and separate diagnosis OK",
  );
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
