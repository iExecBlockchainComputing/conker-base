#!/bin/bash

log_info() {
  echo -e "[INFO] $*"
}

log_fatal() {
  echo -e "[ERROR] $*" >&2
  exit 1
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
    if [ ! -d "$path" ]; then
        mkdir -p "$path"
    else
        umount "$path" 2>/dev/null
    fi

    # this is a new disk, need to partition first
    if [ ! -e "$part_disk" ]; then
        echo -e "n\np\n1\n\n\nw\n" | fdisk "$diskpath"
        mkfs.ext4 "$part_disk"
    fi

    mount "$part_disk" "$path"
    if [ $? -ne 0 ]; then
        log_fatal "Failed to mount $part_disk to $path"
    else
        ls "$path"
    fi
else # keyType is not "none"
    if [ -z "$wrapkey" ]; then
        log_fatal "wrapkey is null"
    fi
    log_info "Mount directory is $path"

    mappername="${disk}1"
    if [ ! -d "$path" ]; then
        mkdir -p "$path"
    fi

    # Try to open LUKS device to check status
    open_info=$(echo "$wrapkey" | cryptsetup luksOpen "$part_disk" testname 2>&1)
    open_exit_code=$?
    
    if [ $open_exit_code -eq 0 ]; then
        log_info "cryptsetup luksOpen $part_disk: success"
        cryptsetup close testname 2>/dev/null
    else
        log_info "cryptsetup luksOpen $part_disk: $open_info"
        
        if echo "$open_info" | grep -q "already mapped or mounted"; then
            log_info "cryptsetup luksOpen $part_disk: $part_disk already correctly mapped to testname"
            exit 0
        elif echo "$open_info" | grep -q "not a valid LUKS device"; then
            log_info "cryptsetup luksOpen $part_disk: $part_disk is not a valid LUKS device"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "doesn't exist or access denied"; then
            log_info "cryptsetup luksOpen $part_disk: $part_disk does not exist or access denied"
            log_info "Encrypting new disk of $diskpath"
            echo -e "n\np\n1\n\n\nw\n" | fdisk "$diskpath"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "No key available"; then
            log_fatal "cryptsetup luksOpen $part_disk: wrong passphrase"
        else
            log_fatal "cryptsetup luksOpen $part_disk: unknown error"
        fi
    fi
    
    # Open the encrypted device
    echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
    if [ $? -ne 0 ]; then
        log_fatal "Fail to execute 'cryptsetup open $part_disk $mappername'"
    fi
    
    # Mount the device
    mount "/dev/mapper/$mappername" "$path"
    if [ $? -ne 0 ]; then
        log_fatal "Failed to mount /dev/mapper/$mappername to $path"
    else
        ls "$path"
    fi
fi

log_info "Mount directory is $path"

log_info "Encrypted disk configuration completed."
