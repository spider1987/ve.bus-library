const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const source = fs.readFileSync("examples/mqtt_ha/web_ui.h", "utf8");
const login = source.slice(
  source.indexOf("static const char LOGIN_HTML"),
  source.indexOf("static const char ADMIN_HTML"),
);
const script = login.match(/<script>([\s\S]*?)<\/script>/)[1];
(async () => {
  for (const [remember, status] of [
    [true, 200],
    [false, 200],
    [true, 401],
    [true, 429],
  ]) {
    const els = {
      login: {},
      submit: { disabled: false },
      error: { textContent: "" },
    };
    let posted, redirect;
    const context = {
      document: { getElementById: (id) => els[id] },
      URLSearchParams,
      FormData: class {
        constructor() {
          return [
            ["username", "admin"],
            ["password", "test-only-password"],
          ];
        }
      },
      fetch: async (url, options) => {
        posted = { url, body: options.body };
        return {
          ok: status === 200,
          json: async () => ({ error: "Testfehler" }),
        };
      },
      location: { replace: (url) => (redirect = url) },
    };
    vm.createContext(context);
    vm.runInContext(script, context);
    await els.login.onsubmit({
      preventDefault() {},
      target: { remember: { checked: remember } },
    });
    assert.equal(posted.url, "/auth/login");
    assert.equal(posted.body.get("remember"), remember ? "1" : "0");
    if (status === 200) assert.equal(redirect, "/admin/");
    else {
      assert.equal(redirect, undefined);
      assert.equal(els.submit.disabled, false);
      assert.equal(els.error.textContent, "Testfehler");
    }
  }
  assert(!login.includes("localStorage"));
  console.log(
    "Login UI: remembered/session login, wrong password, rate limit OK",
  );
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
