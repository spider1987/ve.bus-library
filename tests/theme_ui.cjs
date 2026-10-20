const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const source = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const script = source.match(
  /THEME_JS\[\].*?R"rawliteral\(([\s\S]*?)\)rawliteral";/,
)[1];
function run(saved, blocked = false) {
  const listeners = {},
    select = {
      value: "",
      addEventListener: (name, fn) => (listeners[name] = fn),
    };
  let value = saved,
    writes = 0,
    redraws = 0;
  const context = {
    document: {
      documentElement: { dataset: {} },
      getElementById: () => select,
      addEventListener: (name, fn) => (listeners[name] = fn),
    },
    localStorage: {
      getItem() {
        if (blocked) throw Error("blocked");
        return value;
      },
      setItem(k, v) {
        if (blocked) throw Error("blocked");
        assert.equal(k, "vebus-theme");
        value = v;
        writes++;
      },
    },
    Event: class {
      constructor(type) {
        this.type = type;
      }
    },
    window: {
      dispatchEvent: (e) => {
        assert.equal(e.type, "themechange");
        redraws++;
      },
      addEventListener: (name, fn) => (listeners[name] = fn),
    },
  };
  vm.createContext(context);
  vm.runInContext(script, context);
  listeners.DOMContentLoaded();
  return {
    context,
    select,
    listeners,
    get value() {
      return value;
    },
    get writes() {
      return writes;
    },
    get redraws() {
      return redraws;
    },
  };
}
for (const theme of ["oled", "midnight", "graphite"]) {
  const test = run(theme);
  assert.equal(test.context.document.documentElement.dataset.theme, theme);
  assert.equal(test.select.value, theme);
  test.select.value = "graphite";
  test.listeners.change();
  assert.equal(test.value, "graphite");
  assert.equal(test.writes, 1);
  assert.equal(test.redraws, 2);
  test.listeners.storage({ key: "vebus-theme", newValue: "midnight" });
  assert.equal(test.select.value, "midnight");
  assert.equal(test.writes, 1);
}
for (const saved of [null, "invalid", "<script>"])
  assert.equal(run(saved).select.value, "oled");
const blocked = run("midnight", true);
blocked.select.value = "graphite";
blocked.listeners.change();
assert.equal(
  blocked.context.document.documentElement.dataset.theme,
  "graphite",
);
for (const name of ["INDEX_HTML", "ADMIN_HTML", "LOGIN_HTML", "INFO_HTML"]) {
  const page = source.match(
    new RegExp(name + '\\[\\].*?R"rawliteral\\(([\\s\\S]*?)\\)rawliteral";'),
  )[1];
  assert(page.indexOf("/theme.js") < page.indexOf("/style.css"));
  assert(page.includes('id="themeSelect"'));
}
console.log(
  "Themes: selection, persistence, storage failure, cross-tab sync and redraw OK",
);
