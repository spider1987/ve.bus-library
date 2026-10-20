const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const s = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const script = s.slice(
  s.indexOf("let autoActive=false"),
  s.indexOf("let acCanSetPower=false"),
);
(async () => {
  for (const [enabled, fresh, ro] of [
    [false, true, false],
    [true, true, false],
    [true, false, false],
    [false, true, true],
  ]) {
    const els = {};
    const get = (id) => (els[id] ??= { disabled: false, textContent: "" });
    const a = {
      share_pct: 90,
      charge_limit_w: 3000,
      discharge_limit_w: 3000,
      cutoff_v: 47,
      resume_delta_v: 1,
      discharge_blocked: false,
      enabled,
      fresh,
      status: "Teststatus",
      target_w: 1800,
      setpoint_w: 1500,
      demand_w: 2000,
      byd_w: 200,
      victron_w: 1500,
    };
    const c = {
      $: get,
      fetch: async () => ({ ok: true, json: async () => a }),
      post: async () => {},
      toast: () => {},
      state: async () => {},
    };
    vm.createContext(c);
    vm.runInContext(script, c);
    vm.runInContext("autoReadOnly=" + ro, c);
    await vm.runInContext("autoPoll()", c);
    assert.equal(get("essSet").disabled, enabled || ro);
    assert.equal(get("vmSet").disabled, enabled || ro);
    assert.equal(get("autoOn").disabled, enabled || ro);
    assert.equal(get("autoOff").disabled, !enabled || ro);
    assert.equal(get("autoDemand").textContent, fresh ? "2.000 W" : "—");
    assert(get("autoStatus").textContent.includes("1.800 W"));
  }
  console.log("Auto UI modes, stale values and manual control locking OK");
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
