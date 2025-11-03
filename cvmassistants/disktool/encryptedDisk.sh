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
        log_fatal "Fail to mount"
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
        log_info "open luks disk successful"
        cryptsetup close testname 2>/dev/null
    else
        log_info "luksOpen $part_disk: $open_info"
        
        if echo "$open_info" | grep -q "already mapped or mounted"; then
            log_info "already correctly mounted"
            exit 0
        elif echo "$open_info" | grep -q "not a valid LUKS device"; then
            log_info "Not a LUKS device"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "doesn't exist or access denied"; then
            log_info "Not an existing device"
            log_info "Encrypting new disk of $diskpath"
            echo -e "n\np\n1\n\n\nw\n" | fdisk "$diskpath"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "No key available"; then
            log_fatal "Password error"
        else
            log_fatal "Could not know the status"
        fi
    fi
    
    # Open the encrypted device
    echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
    if [ $? -ne 0 ]; then
        log_fatal "Fail to execute cryptsetup open"
    fi
    
    # Mount the device
    mount "/dev/mapper/$mappername" "$path"
    if [ $? -ne 0 ]; then
        log_fatal "Fail to mount"
    else
        ls "$path"
    fi
fi

log_info "Mount directory is $path"

log_info "Encrypted disk configuration completed."
