const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const src = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const a = src.indexOf("async function energyPoll()"),
  b = src.indexOf("energyPoll();setInterval", a);
const fn = src.slice(a, b);
(async () => {
  for (const valid of [true, false]) {
    const els = {};
    const state = {
      date: "2026-10-18",
      time_valid: valid,
      fresh: valid,
      storage_ok: valid,
      today_charge_kwh: valid ? 3 : null,
      today_discharge_kwh: valid ? 2.4 : null,
      total_charge_kwh: 30,
      total_discharge_kwh: 24,
      today_measured_s: 3600,
      unassigned_charge_kwh: valid ? 0 : 1,
      unassigned_discharge_kwh: 0,
      mqtt_topic: "vebus/multiplus/energy",
      days: [
        {
          date: "2026-10-18",
          charged_kwh: 3,
          discharged_kwh: 2.4,
          measured_s: 3600,
        },
      ],
    };
    const c = {
      $: (id) => (els[id] ??= {}),
      fetch: async () => ({ ok: true, json: async () => state }),
    };
    vm.createContext(c);
    await vm.runInContext(fn + "energyPoll()", c);
    assert.equal(
      els.energyTodayDischarge.textContent,
      valid ? "2,400 kWh" : "—",
    );
    assert.equal(els.energyTotalDischarge.textContent, "24,000 kWh");
    assert.equal(els.energyMqtt.textContent, state.mqtt_topic);
    assert(els.energyDays.innerHTML.includes("2,400"));
    if (!valid)
      assert(els.energyNote.textContent.includes("Speichern fehlgeschlagen"));
  }
  console.log(
    "Energy UI: daily values, archive, missing clock, storage error and MQTT topic OK",
  );
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
