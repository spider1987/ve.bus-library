from pathlib import Path
for name in ("examples/mqtt_ha/web_ui.h", "examples/mqtt_ha/main.cpp"):
    text = Path(name).read_bytes().decode("utf-8", errors="strict")
    assert "\ufffd" not in text, name
print("Web UI and response strings: valid UTF-8, no replacement characters")
