from pathlib import Path
import re
import subprocess

Import("env")


def read_fw_version():
    config_path = Path(env["PROJECT_DIR"]) / "include" / "config.h"
    text = config_path.read_text(encoding="utf-8")
    match = re.search(r'#define\s+APP_VERSION_BASE\s+"([^"]+)"', text)
    if not match:
        raise ValueError("APP_VERSION_BASE konnte in include/config.h nicht gefunden werden")
    return match.group(1)


def git_output(args):
    try:
        result = subprocess.run(
            ["git", "-C", env["PROJECT_DIR"], *args],
            check=True,
            capture_output=True,
            text=True,
        )
        return result.stdout.strip()
    except Exception:
        return ""


def build_label(version: str) -> str:
    short_sha = git_output(["rev-parse", "--short", "HEAD"]) or "unknown"
    exact_tag = git_output(["describe", "--tags", "--exact-match", "HEAD"])
    if exact_tag in {version, f"v{version}"}:
        return version
    return f"{version}+{short_sha}"


version = read_fw_version()
label = build_label(version)
generated_path = Path(env["PROJECT_DIR"]) / "include" / "version_generated.h"
generated_path.write_text(
    "#pragma once\n"
    "// Automatisch durch scripts/version_label.py erzeugt. Nicht manuell pflegen.\n"
    f"#define APP_VERSION_BASE \"{version}\"\n"
    f"#define FW_VERSION \"{label}\"\n",
    encoding="utf-8",
)

env["FW_VERSION_LABEL"] = label
env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % label)])
