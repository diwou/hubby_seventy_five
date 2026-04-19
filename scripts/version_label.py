from pathlib import Path
import re
import subprocess

Import("env")


def read_fw_version():
    config_path = Path(env["PROJECT_DIR"]) / "src" / "config.h"
    text = config_path.read_text(encoding="utf-8")
    match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', text)
    if not match:
        raise ValueError("FW_VERSION konnte in src/config.h nicht gefunden werden")
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

env["FW_VERSION_LABEL"] = label
env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % label)])
