# File:    extra_script.py
# Author:  Trung Nguyen
# GitHub:  https://github.com/trungnguyen278/PTalk-V2
# Date:    30 Jun 2026
#
# Description:
#  - Part of the PTalk-V2 project
#  - Written and maintained by Trung Nguyen

Import("env")

import shutil
from pathlib import Path

def get_version():
    version_file = Path(env["PROJECT_SRC_DIR"]) / "Version.hpp"
    if not version_file.exists():
        return "unknown"

    with open(version_file, "r", encoding="utf-8") as f:
        for line in f:
            if "APP_VERSION" in line and '"' in line:
                return line.split('"')[1]
    return "unknown"


def after_build(source, target, env):
    build_dir = Path(env["PROJECT_DIR"]) / "build"
    build_dir.mkdir(exist_ok=True)

    version = get_version()

    firmware_src = Path(target[0].get_abspath())
    firmware_dst = build_dir / f"{version}.bin"

    shutil.copy(firmware_src, firmware_dst)
    print(f"\n✓ Firmware copied to: {firmware_dst}\n")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
