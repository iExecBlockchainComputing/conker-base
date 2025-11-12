#!/usr/bin/env bash
###############################################################################
# Script: encryptedDisk.sh
# Description: Configure encrypted or unencrypted disk partitions on Ubuntu systems (e.g., TDX environment)
#
# This script partitions, formats, and mounts disk devices. Supports both
# encrypted (LUKS) and unencrypted disks. Environment variables control behavior:
# `MOUNT_PATH` (mount point), `DISK` (device name), `KEY_TYPE` (only wrapkey supported),
# and `WRAP_KEY` (encryption key).
#
# Requirements:
#   - Must be run as root
#   - cryptsetup must be installed (for encrypted disks)
#   - fdisk must be installed
#   - mkfs.ext4 must be available
#
###############################################################################

log_info() {
  echo -e "[INFO] $*"
}

log_fatal() {
  echo -e "[ERROR] $*" >&2
  exit 1
}

# Create a new partition on a disk
# Arguments: disk_device
detect_or_create_partition() {
  local disk_dev="$1"
  local suffix

  # Try both possible partition naming schemes (e.g., /dev/sda1 or /dev/nvme0n1p1)
  for suffix in "1" "p1"; do
    if [[ -e "${disk_dev}${suffix}" ]]; then
      part_disk="${disk_dev}${suffix}"
      mappername="${DISK}${suffix}"
      log_info "Partition $part_disk already exists for device $disk_dev"
      return 0
    fi
  done

  log_info "Creating partition on $disk_dev with the following passed fdisk parameters:
  n = new partition
  p = primary partition
  1 = partition number 1
  <Enter><Enter> = default start and end sectors
  w = write changes"
  # Create the partition using fdisk
  # fdisk may return non-zero due to partition table re-read warning, but partition is created
  echo -e "n\np\n1\n\n\nw\n" | fdisk "$disk_dev" >/dev/null 2>&1 || true

  # Force kernel to re-read the partition table
  if command -v partprobe >/dev/null 2>&1; then
    partprobe "$disk_dev" >/dev/null 2>&1 || log_fatal "partprobe failed on $disk_dev"
  elif command -v partx >/dev/null 2>&1; then
    partx -u "$disk_dev" >/dev/null 2>&1 || log_fatal "partx failed on $disk_dev"
  fi

  # Wait a moment for partition to appear
  sleep 1

  # Try both possible partition naming schemes
  for suffix in "1" "p1"; do
    part_disk="${disk_dev}${suffix}"
    if [[ -e "$part_disk" ]]; then 
      mappername="${DISK}${suffix}"
      log_info "Partition $part_disk successfully created on $disk_dev"
      return 0
    fi
  done

  log_fatal "Failed to create partition on $disk_dev — no partition device detected after fdisk"
}

# Format and encrypt a partition
# Arguments: key partition_device mapper_name
format_and_encrypt_partition() {
  local key="$1"
  local part_dev="$2"
  local mapper="$3"

  echo "$key" | cryptsetup luksFormat --key-file=- "$part_dev"
  [[ $? -ne 0 ]] && log_fatal "Failed to format partition $part_dev in luks format"
  log_info "Partition $part_dev formatted successfully in luks format"

  echo "$key" | cryptsetup open --key-file=- "$part_dev" "$mapper"
  [[ $? -ne 0 ]] && log_fatal "Failed to open partition $part_dev in luks format"
  log_info "Partition $part_dev opened successfully in luks format"

  mkfs.ext4 "/dev/mapper/$mapper"
  [[ $? -ne 0 ]] && log_fatal "Failed to format partition /dev/mapper/$mapper in ext4 format"
  log_info "Partition /dev/mapper/$mapper successfully formatted in ext4 format"

  cryptsetup close "$mapper"
  [[ $? -ne 0 ]] && log_fatal "Failed to close partition /dev/mapper/$mapper"
  log_info "Partition /dev/mapper/$mapper closed successfully"
}

# Mount a device to a mount point
# Arguments: device_path mount_point
mount_device() {
  local device="$1"
  local mount_point="$2"

  mount "$device" "$mount_point"
  [[ $? -ne 0 ]] && log_fatal "Failed to mount $device to $mount_point"
  log_info "Mounted $device to $mount_point"
}

log_info "Starting encrypted disk configuration..."

# Check required environment variables
[[ -z "$MOUNT_PATH" ]] && log_fatal "Mount directory is null"
[[ -z "$DISK" ]] && log_fatal "Disk dev name is null"
# Handle only encrypted disk case
[ "$KEY_TYPE" != "wrapkey" ] && log_fatal "KEY_TYPE $KEY_TYPE is not supported"

log_info "Handling encrypted disk case"
[[ -z "$WRAP_KEY" ]] && log_fatal "WRAP_KEY is null"

if [ ! -d "$MOUNT_PATH" ]; then
    log_info "Mount directory $MOUNT_PATH does not exist"
    mkdir -p "$MOUNT_PATH" && log_info "Created mount directory $MOUNT_PATH"
else
    umount "$MOUNT_PATH" 2>/dev/null && log_info "Unmounted $MOUNT_PATH"
fi

diskpath="/dev/$DISK" # /dev/vda
part_disk=""
mappername=""
detect_or_create_partition "$diskpath" # assign part_disk and mappername
device_to_mount="/dev/mapper/$mappername"
[ -e "$device_to_mount"  ] && log_fatal "Mapper $device_to_mount already exists"

# Format and encrypt the partition (and check if it opens correctly)
format_and_encrypt_partition "$WRAP_KEY" "$part_disk" "$mappername"

# Open the encrypted device in its mapper
echo "$WRAP_KEY" | cryptsetup open --key-file=- "$part_disk" "$mappername"
[[ $? -ne 0 ]] && log_fatal "cryptsetup open --key-file=- "$part_disk" "$mappername": failed"
log_info "cryptsetup open --key-file=- "$part_disk" "$mappername": success"

# Mount the device
mount_device "$device_to_mount" "$MOUNT_PATH" && log_info "Mounted $device_to_mount to $MOUNT_PATH"

log_info "Encrypted disk configuration completed."
