const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const s = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8"),
  script = s.slice(
    s.indexOf("let autoActive=false"),
    s.indexOf("let acCanSetPower=false"),
  );
(async () => {
  for (const [byd, wait, valid] of [
    [100, 35, true],
    [0, 5, true],
    [1000, 120, true],
    [-1, 35, false],
    [1001, 35, false],
    [100, 4, false],
    [100, 121, false],
    [100, 35.5, false],
  ]) {
    let posted = null;
    const els = {
      autoSleepAtCutoff: { checked: false },
      autoBydMinSoc: { value: "80" },autoWakeMinW: { value: "200" },autoWakeDelay: { value: "30" },
      autoBydThreshold: { value: String(byd) },
      autoDataTimeout: { value: String(wait) },
      autoSharePct: { value: "90" },
      autoChargeLimit: { value: "3000" },
      autoDischargeLimit: { value: "3000" },
      autoCutOn: { checked: false },
      autoDeltaV: { value: "1" },
    };
    const a = {
      byd_threshold_w: 100,
      data_timeout_s: 35,
      enabled: false,
      fresh: true,
      status: "Aus",
      target_w: 0,
      setpoint_w: 0,
      share_pct: 90,
      charge_limit_w: 3000,
      discharge_limit_w: 3000,
      cutoff_v: 0,
      resume_delta_v: 1,
      discharge_blocked: false,
      demand_w: 0,
      byd_w: 0,
      victron_w: 0,
    };
    const c = {
      $: (id) => (els[id] ??= {}),
      toast: () => {},
      post: async (url, data) => {
        posted = data;
      },
      fetch: async () => ({ ok: true, json: async () => a }),
      state: async () => {},
    };
    vm.createContext(c);
    vm.runInContext(script, c);
    await els.autoSettings.onsubmit({ preventDefault() {} });
    if (valid) {
      assert.equal(posted.byd_threshold_w, byd);assert.equal(posted.byd_min_soc,80);assert.equal(posted.wake_min_w,200);assert.equal(posted.wake_delay_s,30);assert.equal(posted.sleep_at_cutoff,0);
      assert.equal(posted.data_timeout_s, wait);
    } else assert.equal(posted, null);
  }
  console.log("BYD threshold and timeout: valid bounds and invalid values OK");
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
