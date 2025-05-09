import os
import subprocess
import sys

path = os.getenv('path')
if path is None:
    print('error:  mount directory is null')
    sys.exit(1)

disk = os.getenv('disk')
if disk is None:
    print('error: disk dev name is null')
    sys.exit(1)
diskpath = '/dev/' + disk
part_disk= diskpath + '1'
keyType = os.getenv('keyType')
if keyType == "none" :
    if not os.path.exists(path):
        os.makedirs(path)
    else:
        cmd = 'umount ' + path
        ret = subprocess.run(cmd, shell=True)

    # this is a new disk, need to encrypt first
    if not os.path.exists(part_disk):
        ret = subprocess.run('fdisk ' + diskpath, input="n\np\n1\n\n\nw\n".encode(), shell=True)
        ret = subprocess.run('mkfs.ext4 ' + part_disk, shell=True)

    ret = subprocess.run('mount ' + part_disk + ' ' + path, shell=True)
    if ret.returncode:
        print('fail to mount')
        sys.exit(2)
    else:
        print(os.listdir(path))
else:
    key = os.getenv('wrapkey')
    if key is None:
        print('error: wrapkey is null')
        sys.exit(1)
    print("mount directory is " + path)

    mappername = disk + '1'
    if not os.path.exists(path):
        os.makedirs(path)

    try:
        # CONFIG_VIRTIO_BLK=y, CONFIG_VIRTIO_NET=y, CONFIG_DM_CRYPT=y
        open_info = subprocess.check_output("cryptsetup " + "luksOpen " +  part_disk + " testname", shell=True, stderr=subprocess.STDOUT, universal_newlines=True,input=key)
        print("open luks disk successful")
        ret = subprocess.run('cryptsetup close ' + "testname", shell=True)
    except subprocess.CalledProcessError as e :
        print(e.output)
        open_info=e.output
        if "already mapped or mounted" in open_info:
            print("already correct mounted")
            sys.exit(0)
            ##todo
        elif "not a valid LUKS device" in open_info:
            print("not a LUKS device")
            ret = subprocess.run('cryptsetup luksFormat ' + part_disk, input=key.encode(), shell=True)
            ret = subprocess.run('cryptsetup open ' + part_disk + ' ' + mappername, input=key.encode(), shell=True)
            ret = subprocess.run('mkfs.ext4 /dev/mapper/' + mappername, shell=True)
            ret = subprocess.run('cryptsetup close ' + mappername, shell=True)
        elif "doesn't exist or access denied" in open_info:
            print("not a exist device")
            print('encrypt a new disk of ' + diskpath)
            ret = subprocess.run('fdisk ' + diskpath, input="n\np\n1\n\n\nw\n".encode(), shell=True)
            ret = subprocess.run('cryptsetup luksFormat ' + part_disk, input=key.encode(), shell=True)
            ret = subprocess.run('cryptsetup open ' + part_disk + ' ' + mappername, input=key.encode(), shell=True)
            ret = subprocess.run('mkfs.ext4 /dev/mapper/' + mappername, shell=True)
            ret = subprocess.run('cryptsetup close ' + mappername, shell=True)
        elif "No key available" in open_info:
            print("password error")
            sys.exit(2)
        else:
            print("could not know the status")
            sys.exit(3)
    ret = subprocess.run('cryptsetup open ' + part_disk + ' ' + mappername, input=key.encode(), shell=True)
    if ret.returncode:
        print('fail to cryptsetup open')
        sys.exit(1)
    mnt_cmd = 'mount /dev/mapper/' + mappername + ' ' + path
    ret = subprocess.run(mnt_cmd, shell=True)
    if ret.returncode:
        print('fail to mount')
        sys.exit(2)
    else:
        print(os.listdir(path))
print("mount dir succussful")