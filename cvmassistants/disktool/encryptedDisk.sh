#!/usr/bin/env bash
###############################################################################
# Script: encryptedDisk.sh
# Description: Configure encrypted or unencrypted disk partitions on Ubuntu systems (e.g., TDX environment)
#
# This script partitions, formats, and mounts disk devices. Supports both
# encrypted (LUKS) and unencrypted disks. Environment variables control behavior:
# `path` (mount point), `disk` (device name), `keyType` (encryption type),
# and `wrapkey` (encryption key when keyType is not "none").
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
  part_disk=""
  for suffix in 1 p1; do
    if [[ -e "${disk_dev}${suffix}" ]]; then
      part_disk="${disk_dev}${suffix}"
      break
    fi
  done

  if [[ -n "$part_disk" ]]; then
    log_info "Partition $part_disk already exists for device $disk_dev"
    return 0
  fi

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
    [[ -e "$part_disk" ]] && break
  done

  [[ -e "$part_disk" ]] || log_fatal "Failed to create partition on $disk_dev — no partition device detected after fdisk"
  log_info "Partition $part_disk successfully created on $disk_dev"
  return 0
}

# Format and encrypt a partition
# Arguments: key partition_device mapper_name
format_and_encrypt_partition() {
  local key="$1"
  local part_dev="$2"
  local mapper="$3"
  
  echo "$key" | cryptsetup luksFormat "$part_dev"
  [[ $? -ne 0 ]] && log_fatal "Failed to format partition $part_dev in luks format"
  log_info "Partition $part_dev formatted successfully in luks format"

  echo "$key" | cryptsetup open "$part_dev" "$mapper"
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
[[ -z "$path" ]] && log_fatal "Mount directory is null"
[[ -z "$disk" ]] && log_fatal "Disk dev name is null"

diskpath="/dev/$disk" # /dev/vda
part_disk=""
detect_or_create_partition "$diskpath" # assign part_disk

# Handle unencrypted disk case
if [ "$keyType" == "none" ]; then
    log_info "Handling unencrypted disk case"
    if [ ! -d "$path" ]; then
        log_info "Mount directory $path does not exist"
        mkdir -p "$path" && log_info "Created mount directory $path"
    else
        umount "$path" 2>/dev/null && log_info "Unmounted $path"
    fi

    # this is a new disk, need to partition first
    mkfs.ext4 "$part_disk" && log_info "Formatted partition $part_disk in ext4 format"
    device_to_mount="$part_disk"

else # keyType is NOT "none" (wrapkey)
    log_info "Handling encrypted disk case"
    [[ -z "$wrapkey" ]] && log_fatal "wrapkey is null"

    mappername="${disk}1"
    if [ ! -d "$path" ]; then
        log_info "Mount directory $path does not exist"
        mkdir -p "$path" && log_info "Created mount directory $path"
    fi

    # Try to open LUKS device on "testname" to anticipate errors
    open_info=$(echo "$wrapkey" | cryptsetup open "$part_disk" testname 2>&1)
    open_exit_code=$?
    
    if [ $open_exit_code -eq 0 ]; then
        log_info "cryptsetup open $part_disk testname: success"
        cryptsetup close testname 2>/dev/null && log_info "cryptsetup closed testname: success"
    else
        log_info "cryptsetup open $part_disk testname: $open_info"
        
        if echo "$open_info" | grep -qi "already exists"; then
            log_info "cryptsetup open $part_disk testname: $part_disk already correctly mapped to testname"
            exit 0
        elif echo "$open_info" | grep -qi "not a valid LUKS device"; then
            log_info "cryptsetup open $part_disk testname: $part_disk is not a valid LUKS device"
            format_and_encrypt_partition "$wrapkey" "$part_disk" "$mappername"
        elif echo "$open_info" | grep -qi "No key available"; then
            log_fatal "cryptsetup open $part_disk testname: wrong passphrase"
        else
            log_fatal "cryptsetup open $part_disk testname: unknown error"
        fi
    fi
    
    # Open the encrypted device
    echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
    [[ $? -ne 0 ]] && log_fatal "cryptsetup open $part_disk $mappername: failed"
    log_info "cryptsetup open $part_disk $mappername: success"
    
    device_to_mount="/dev/mapper/$mappername"
fi

# Mount the device
log_info "Device to mount is $device_to_mount"
mount_device "$device_to_mount" "$path"

log_info "Mount directory is $path"
log_info "List contents of $path"
ls "$path"

log_info "Encrypted disk configuration completed."
