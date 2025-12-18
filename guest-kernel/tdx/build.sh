set -e

# Clone Ubuntu HWE 6.17 (has RTMR extend built-in)
git clone -b hwe-6.17-next --single-branch --depth 1 \
    https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble linux

cd linux
cp ../tdx/config .config

apt install -y libelf-dev zstd flex bison libssl-dev bc

# virt coco，for enable tdx 
./scripts/config --enable CONFIG_VIRT_DRIVERS # important to enable: parent menu that gates all confidential computing drivers
./scripts/config --enable CONFIG_EFI_SECRET
./scripts/config --enable CONFIG_INTEL_TDX_GUEST
./scripts/config --enable CONFIG_TDX_GUEST_DRIVER
#enable dmcrypt for encrypt disk
./scripts/config --enable CONFIG_DM_CRYPT
#enable ramfs and initrd for all in ram
./scripts/config --enable CONFIG_BLK_DEV_INITRD
./scripts/config --enable CONFIG_BLK_DEV_RAM
# VSOCK modules
./scripts/config --enable CONFIG_VSOCKETS        # required for communication with QGS (quote generation)
./scripts/config --enable CONFIG_VIRTIO_VSOCKETS  
./scripts/config --enable CONFIG_VIRTIO_VSOCKETS_COMMON
./scripts/config --enable CONFIG_VSOCKETS_LOOPBACK
# Netfilter / nftables (IPv4 only, for ufw with iptables-nft backend)
./scripts/config --enable CONFIG_NETFILTER
./scripts/config --enable CONFIG_NETFILTER_ADVANCED
./scripts/config --enable CONFIG_NETFILTER_XTABLES
./scripts/config --enable CONFIG_NF_CONNTRACK
./scripts/config --enable CONFIG_NF_TABLES
./scripts/config --enable CONFIG_NFT_COMPAT
./scripts/config --enable CONFIG_NFT_CT
./scripts/config --enable CONFIG_NFT_LOG
./scripts/config --enable CONFIG_NFT_LIMIT
./scripts/config --enable CONFIG_NFT_NAT
./scripts/config --enable CONFIG_NFT_MASQ
./scripts/config --enable CONFIG_NFT_REDIR
./scripts/config --enable CONFIG_NFT_REJECT
./scripts/config --enable CONFIG_NFT_FIB
# IPv4 nftables
./scripts/config --enable CONFIG_NF_TABLES_IPV4
./scripts/config --enable CONFIG_NFT_REJECT_IPV4
./scripts/config --enable CONFIG_NFT_FIB_IPV4
./scripts/config --enable CONFIG_NFT_DUP_IPV4
./scripts/config --enable CONFIG_NF_NAT
./scripts/config --enable CONFIG_IP_NF_IPTABLES
./scripts/config --enable CONFIG_IP_NF_TARGET_REJECT
./scripts/config --enable CONFIG_IP_NF_TARGET_MASQUERADE

yes "" | make olddefconfig
make -j$(nproc) bzImage
