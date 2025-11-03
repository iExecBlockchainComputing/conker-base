#!/bin/bash

# Check required environment variables
if [ -z "$path" ]; then # /workplace/encryptedData/ 
    echo 'error:  mount directory is null'
    exit 1
fi

if [ -z "$disk" ]; then # vda
    echo 'error: disk dev name is null'
    exit 1
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
        echo 'fail to mount'
        exit 2
    else
        ls "$path"
    fi
else # keyType is not "none"
    if [ -z "$wrapkey" ]; then
        echo 'error: wrapkey is null'
        exit 1
    fi
    echo "mount directory is $path"

    mappername="${disk}1"
    if [ ! -d "$path" ]; then
        mkdir -p "$path"
    fi

    # Try to open LUKS device to check status
    open_info=$(echo "$wrapkey" | cryptsetup luksOpen "$part_disk" testname 2>&1)
    open_exit_code=$?
    
    if [ $open_exit_code -eq 0 ]; then
        echo "open luks disk successful"
        cryptsetup close testname 2>/dev/null
    else
        echo "$open_info"
        
        if echo "$open_info" | grep -q "already mapped or mounted"; then
            echo "already correct mounted"
            exit 0
        elif echo "$open_info" | grep -q "not a valid LUKS device"; then
            echo "not a LUKS device"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "doesn't exist or access denied"; then
            echo "not a exist device"
            echo "encrypt a new disk of $diskpath"
            echo -e "n\np\n1\n\n\nw\n" | fdisk "$diskpath"
            echo "$wrapkey" | cryptsetup luksFormat "$part_disk"
            echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
            mkfs.ext4 "/dev/mapper/$mappername"
            cryptsetup close "$mappername"
        elif echo "$open_info" | grep -q "No key available"; then
            echo "password error"
            exit 2
        else
            echo "could not know the status"
            exit 3
        fi
    fi
    
    # Open the encrypted device
    echo "$wrapkey" | cryptsetup open "$part_disk" "$mappername"
    if [ $? -ne 0 ]; then
        echo 'fail to cryptsetup open'
        exit 1
    fi
    
    # Mount the device
    mount "/dev/mapper/$mappername" "$path"
    if [ $? -ne 0 ]; then
        echo 'fail to mount'
        exit 2
    else
        ls "$path"
    fi
fi

echo "mount dir succussful"

