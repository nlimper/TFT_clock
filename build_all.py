import os
import subprocess
import datetime
import json

# Define build environments and their corresponding metadata.
environments = {
    "PCB_v1_round_240x240": {
        "name": "QuadClock Orbix",
        "littlefs_offset": 4259840,
    },
    "PCB_v1_169_240x280": {
        "name": "QuadClock Bricta",
        "littlefs_offset": 4259840,
    },
    "PCB_v1_350_320x480": {
        "name": "QuadClock Grandis",
        "littlefs_offset": 4259840,
    }
}

# Get current date in YYMMDD format.
date_str = datetime.datetime.now().strftime("%y%m%d")

# Relative paths where PlatformIO outputs compiled files.
firmware_rel_path = ".pio/build/{env}/firmware.bin"
littlefs_rel_path = ".pio/build/{env}/littlefs.bin"
bootloader_rel_path = ".pio/build/{env}/bootloader.bin"
partitions_rel_path = ".pio/build/{env}/partitions.bin"

# Output directory for renamed binaries.
output_dir = "firmware"
os.makedirs(output_dir, exist_ok=True)

# JSON output directory.
json_output_dir = "firmware"
os.makedirs(json_output_dir, exist_ok=True)

json_entries = []

for env_name, metadata in environments.items():
    print(f"\n=== Processing {env_name} ===")

    # 1. Build the filesystem (littlefs) partition image.
    buildfs_cmd = f'platformio run --environment {env_name} --target buildfs'
    print("Running buildfs command:", buildfs_cmd)
    result = subprocess.run(buildfs_cmd, shell=True)
    if result.returncode != 0:
        print(f"❌ buildfs failed for {env_name}. Skipping filesystem image.")
    else:
        src_littlefs = littlefs_rel_path.format(env=env_name)
        if os.path.exists(src_littlefs):
            dest_littlefs = os.path.join(output_dir, f"{date_str}-{env_name}-littlefs.bin")
            os.replace(src_littlefs, dest_littlefs)
            print(f"✅ Saved filesystem image: {dest_littlefs}")
        else:
            print(f"⚠️ Filesystem image not found for {env_name} at {src_littlefs}")

    # Clean cache.
    firmware_cmd = f'platformio run --environment {env_name} --target clean'
    print("Clean cache:", firmware_cmd)
    result = subprocess.run(firmware_cmd, shell=True)
    if result.returncode != 0:
        print(f"❌ Clean failed for {env_name}.")
        continue

    # 2. Build the firmware.
    firmware_cmd = f'platformio run --environment {env_name}'
    print("Running firmware build command:", firmware_cmd)
    result = subprocess.run(firmware_cmd, shell=True)
    if result.returncode != 0:
        print(f"❌ Firmware build failed for {env_name}.")
        continue

    # Copy and rename firmware
    src_firmware = firmware_rel_path.format(env=env_name)
    if os.path.exists(src_firmware):
        dest_firmware = os.path.join(output_dir, f"{date_str}-{env_name}-firmware.bin")
        os.replace(src_firmware, dest_firmware)
        print(f"✅ Saved firmware: {dest_firmware}")
    else:
        print(f"⚠️ Firmware file not found for {env_name} at {src_firmware}")

    # Copy and rename bootloader
    src_bootloader = bootloader_rel_path.format(env=env_name)
    if os.path.exists(src_bootloader):
        dest_bootloader = os.path.join(output_dir, f"{date_str}-{env_name}-bootloader.bin")
        os.replace(src_bootloader, dest_bootloader)
        print(f"✅ Saved bootloader: {dest_bootloader}")
    else:
        print(f"⚠️ Bootloader file not found for {env_name} at {src_bootloader}")

    # Copy and rename partition table
    src_partitions = partitions_rel_path.format(env=env_name)
    if os.path.exists(src_partitions):
        dest_partitions = os.path.join(output_dir, f"{date_str}-{env_name}-partitions.bin")
        os.replace(src_partitions, dest_partitions)
        print(f"✅ Saved partition table: {dest_partitions}")
    else:
        print(f"⚠️ Partition table file not found for {env_name} at {src_partitions}")

    # Add entry for JSON file
    json_data = {
            "name": metadata["name"],
            "version": date_str,
            "new_install_prompt_erase": True,
            "new_install_improv_wait_time": 8,
            "builds": [
                {
                    "chipFamily": "ESP32-S3",
                    "parts": [
                        {"path": f"../firmware/{date_str}-{env_name}-bootloader.bin", "offset": 0},
                        {"path": f"../firmware/{date_str}-{env_name}-partitions.bin", "offset": 32768},
                        {"path": f"../firmware/{date_str}-{env_name}-firmware.bin", "offset": 65536},
                        {"path": f"../firmware/{date_str}-{env_name}-littlefs.bin", "offset": metadata["littlefs_offset"]},
                    ]
                }
            ]
        }

    json_file_path = os.path.join(json_output_dir, f"{date_str}-{env_name}.json")
    with open(json_file_path, "w") as json_file:
        json.dump(json_data, json_file, indent=2)

    print(f"✅ Saved ESP Web Tools JSON for {env_name}: {json_file_path}")

print("\n🎉 Build process completed!")
