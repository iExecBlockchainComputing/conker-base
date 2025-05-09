set -e
git clone -b master --single-branch --depth 1 https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble linux
cd linux
cp ../config .config

apt install -y libelf-dev zstd flex bison libssl-dev bc

# virt coco，for enable tdx and sev
./scripts/config --enable CONFIG_EFI_SECRET
./scripts/config --enable CONFIG_INTEL_TDX_GUEST
./scripts/config --enable CONFIG_SEV_GUEST
./scripts/config --enable CONFIG_TDX_GUEST_DRIVER
#enable dmcrypt for encrypt disk
./scripts/config --enable CONFIG_DM_CRYPT

#enable ramfs and initrd for all in ram
./scripts/config --enable CONFIG_BLK_DEV_INITRD
./scripts/config --enable CONFIG_BLK_DEV_RAM

yes "" | make olddefconfig
make -j$(nproc) bzImage 

