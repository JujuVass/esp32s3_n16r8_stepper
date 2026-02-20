"""
PlatformIO extra script: adds 'coredump-info' target.

Usage:
  pio run -e esp32s3_usb -t coredump-info   # Read & decode coredump from flash

Two-step process (works without full ESP-IDF install):
  1. esptool reads raw coredump partition from flash → temp file
  2. esp-coredump decodes that file offline with GDB

Requires: pip install esp-coredump  (installed once)
"""
import os
import subprocess
import sys

Import("env")

# ---------------------------------------------------------------------------
# Paths resolved from PlatformIO environment
# ---------------------------------------------------------------------------
gdb_path = os.path.join(
    env.PioPlatform().get_package_dir("tool-xtensa-esp-elf-gdb"),
    "bin", "xtensa-esp32s3-elf-gdb.exe",
)

rom_elf = os.path.join(
    env.PioPlatform().get_package_dir("tool-esp-rom-elfs"),
    "esp32s3_rev0_rom.elf",
)

esptool_py = os.path.join(
    env.PioPlatform().get_package_dir("tool-esptoolpy"),
    "esptool.py",
)

# Firmware ELF (built by PlatformIO)
firmware_elf = os.path.join(env.subst("$BUILD_DIR"), "firmware.elf")

# Coredump partition (must match default_16MB.csv)
COREDUMP_OFFSET = 0xFC0000
COREDUMP_SIZE   = 0x40000  # 256 KB

# Temp file for raw coredump binary
coredump_bin = os.path.join(env.subst("$BUILD_DIR"), "coredump.bin")

# Serial port (from platformio.ini upload_port or auto-detect)
def get_port():
    port = env.subst("$UPLOAD_PORT")
    if port:
        return port
    from platformio.device.finder import SerialPortFinder
    return SerialPortFinder(
        board_config=env.BoardConfig(),
        upload_protocol=env.subst("$UPLOAD_PROTOCOL"),
    ).find()


# ---------------------------------------------------------------------------
# Target: coredump-info — read coredump from flash and decode it
# ---------------------------------------------------------------------------
def coredump_info_action(target, source, env):
    port = get_port()
    print(f"\n{'='*70}")
    print(f"  COREDUMP ANALYSIS")
    print(f"  Port: {port}")
    print(f"  ELF:  {firmware_elf}")
    print(f"{'='*70}")

    # --- Step 1: Read raw coredump partition from flash using esptool ---
    print(f"\n📥 Step 1: Reading coredump partition from flash...")
    print(f"   Offset: 0x{COREDUMP_OFFSET:X}  Size: 0x{COREDUMP_SIZE:X} ({COREDUMP_SIZE // 1024} KB)\n")

    read_cmd = [
        sys.executable, esptool_py,
        "--chip", "esp32s3",
        "--port", port,
        "--baud", "921600",
        "read_flash",
        hex(COREDUMP_OFFSET),
        hex(COREDUMP_SIZE),
        coredump_bin,
    ]

    print(f"  > {' '.join(read_cmd)}\n")
    result = subprocess.run(read_cmd, cwd=env.subst("$PROJECT_DIR"))
    if result.returncode != 0:
        print("\n❌ Failed to read flash. Is the ESP32 connected via USB?")
        return result.returncode

    # Quick sanity check: all 0xFF means empty (never crashed or partition erased)
    with open(coredump_bin, "rb") as f:
        header = f.read(16)
    if header == b'\xff' * 16:
        print("\n⚠️  Coredump partition is empty (all 0xFF).")
        print("   The device hasn't crashed yet since last flash, or the partition was erased.")
        return 0

    # --- Step 2: Decode coredump offline with esp-coredump + GDB ---
    print(f"\n🔍 Step 2: Decoding coredump...\n")

    decode_cmd = [
        sys.executable, "-m", "esp_coredump",
        "--chip", "esp32s3",
        "info_corefile",
        "--gdb", gdb_path,
        "--core", coredump_bin,
        "--core-format", "raw",
        "--rom-elf", rom_elf,
        firmware_elf,
    ]

    print(f"  > {' '.join(decode_cmd)}\n")
    result = subprocess.run(decode_cmd, cwd=env.subst("$PROJECT_DIR"))
    if result.returncode != 0:
        print("\n❌ Coredump decode failed. The coredump may be corrupted or from a different firmware.")
        return result.returncode

    print(f"\n✅ Coredump analysis complete. Raw dump saved to: {coredump_bin}")
    return 0


env.AddCustomTarget(
    name="coredump-info",
    dependencies=None,
    actions=coredump_info_action,
    title="Coredump Info",
    description="Read and decode coredump from ESP32 flash via USB",
    always_build=True,
)
