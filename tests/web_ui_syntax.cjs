const fs = require("node:fs");
const vm = require("node:vm");
const assert = require("node:assert/strict");
const source = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const scripts = [...source.matchAll(/<script>([\s\S]*?)<\/script>/g)];
assert.equal(
  scripts.length,
  4,
  "Dashboard, info, login and admin scripts must exist",
);
for (const script of scripts) new vm.Script(script[1]);
console.log("All web UI scripts: syntax OK");

const theme = source.match(
  /THEME_JS\[\].*?R"rawliteral\(([\s\S]*?)\)rawliteral";/,
)[1];
new vm.Script(theme);
