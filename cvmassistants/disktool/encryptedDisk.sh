#!/bin/bash

log_info() {
  echo -e "[INFO] $*"
}

log_fatal() {
  echo -e "[ERROR] $*" >&2
  exit 1
}

# Create a new partition on a disk
# Arguments: disk_device
create_partition() {
  local disk_dev="$1"
  log_info "Creating partition on $disk_dev with the following passed fdisk parameters:
  n = new partition
  p = primary partition
  1 = partition number 1
  <Enter><Enter> = default start and end sectors
  w = write changes"
  echo -e "n\np\n1\n\n\nw\n" | fdisk "$disk_dev"
  if [ $? -ne 0 ]; then
    log_fatal "Failed to create partition on $disk_dev"
  fi
  log_info "Partition created successfully on $disk_dev"
}

# Format and encrypt a partition
# Arguments: key partition_device mapper_name
format_and_encrypt_partition() {
  local key="$1"
  local part_dev="$2"
  local mapper="$3"
  
  echo "$key" | cryptsetup luksFormat "$part_dev"
  if [ $? -ne 0 ]; then
    log_fatal "Failed to format partition $part_dev in luks format"
  fi
  log_info "Partition $part_dev formatted successfully in luks format"

  echo "$key" | cryptsetup open "$part_dev" "$mapper"
  if [ $? -ne 0 ]; then
    log_fatal "Failed to open partition $part_dev in luks format"
  fi
  log_info "Partition $part_dev opened successfully in luks format"

  mkfs.ext4 "/dev/mapper/$mapper"
  if [ $? -ne 0 ]; then
    log_fatal "Failed to format partition /dev/mapper/$mapper in ext4 format"
  fi
  log_info "Partition /dev/mapper/$mapper successfully formatted in ext4 format"
  
  cryptsetup close "$mapper"
  if [ $? -ne 0 ]; then
    log_fatal "Failed to close partition /dev/mapper/$mapper"
  fi
  log_info "Partition /dev/mapper/$mapper closed successfully"
}

log_info "Starting encrypted disk configuration..."

# Check required environment variables
if [ -z "$path" ]; then # /workplace/encryptedData/ 
    log_fatal "Mount directory is null"
fi

if [ -z "$disk" ]; then # vda
    log_fatal "Disk dev name is null"
fi

diskpath="/dev/$disk" # /dev/vda
part_disk="${diskpath}1" # /dev/vda1

# Handle unencrypted disk case
if [ "$keyType" == "none" ]; then
    log_info "Handling unencrypted disk case"
    if [ ! -d "$path" ]; then
        log_info "Mount directory $path does not exist"
        mkdir -p "$path"
        log_info "Created mount directory $path"
    else
        umount "$path" 2>/dev/null
        log_info "Unmounted $path"
    fi

    # this is a new disk, need to partition first
    if [ ! -e "$part_disk" ]; then
        log_info "Partition $part_disk does not exist"
        create_partition "$diskpath"
        log_info "Created partition $part_disk"
        mkfs.ext4 "$part_disk"
        log_info "Formatted partition $part_disk in ext4 format"
    fi

    mount "$part_disk" "$path"
    if [ $? -ne 0 ]; then
        log_fatal "Failed to mount $part_disk to $path"
    fi
    log_info "Mounted $part_disk to $path"
    log_info "List contents of $path"
    ls "$path"
    
else # keyType is NOT "none"
    log_info "Handling encrypted disk case"
    if [ -z "$wrapkey" ]; then
        log_fatal "wrapkey is null"
    fi

    mappername="${disk}1"
    if [ ! -d "$path" ]; then
        log_info "Mount directory $path does not exist"
        mkdir -p "$path"
        log_info "Created mount directory $path"
    fi

    # Try to open LUKS device to check status
    open_info=$(echo "$wrapkey" | cryptsetup luksOpen "$part_disk" testname 2>&1)
    open_exit_code=$?
    
    if [ $open_exit_code -eq 0 ]; then
        log_info "cryptsetup luksOpen $part_disk testname: success"
        cryptsetup close testname 2>/dev/null
        log_info "cryptsetup closed testname: success"
    else
        log_info "cryptsetup luksOpen $part_disk testname: $open_info"
        
        if echo "$open_info" | grep -q "already mapped or mounted"; then
            log_info "cryptsetup luksOpen $part_disk testname: $part_disk already correctly mapped to testname"
            exit 0
        elif echo "$open_info" | grep -q "not a valid LUKS device"; then
            log_info "cryptsetup luksOpen $part_disk testname: $part_disk is not a valid LUKS device"
            format_and_encrypt_partition "$wrapkey" "$part_disk" "$mappername"
        elif echo "$open_info" | grep -q "doesn't exist or access denied"; then
            log_info "cryptsetup luksOpen $part_disk testname: $part_disk does not exist or access denied"
            log_info "Encrypting new disk of $diskpath"
            create_partition "$diskpath"
            format_and_encrypt_partition "$wrapkey" "$part_disk" "$mappername"
        elif echo "$open_info" | grep -q "No key available"; then
            log_fatal "cryptsetup luksOpen $part_disk testname: wrong passphrase"
        else
            log_fatal "cryptsetup luksOpen $part_disk: unknown error"
        fi
    fi
    
    # Open the encrypted device
    echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
    if [ $? -ne 0 ]; then
        log_fatal "cryptsetup open $part_disk $mappername: failed"
    fi
    log_info "cryptsetup open $part_disk $mappername: success"
    
    # Mount the device
    mount "/dev/mapper/$mappername" "$path"
    if [ $? -ne 0 ]; then
        log_fatal "Failed to mount /dev/mapper/$mappername to $path"
    fi
    log_info "Mounted /dev/mapper/$mappername to $path"
    log_info "List contents of $path"
    ls "$path"
fi

log_info "Mount directory is $path"

log_info "Encrypted disk configuration completed."
