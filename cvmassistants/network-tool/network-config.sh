#!/usr/bin/env bash

###############################################################################
# Script: network-config.sh
# Description: Configure network interface on Ubuntu system
#
# This script configures a network interface with static IP configuration
# on Ubuntu systems (TDX Trusted Domain Environment).
#
# Prerequisites:
#   - Must be run with root privileges
#   - Must run on Ubuntu OS (TDX Trusted Domain Environment)
#
# Environment Variables Required:
#   - IF_NAME: Network interface name (e.g., eth0)
#   - IF_IP: IP address to assign to the interface
#   - IF_NETMASK: Network subnet mask
#   - IF_GATEWAY: Gateway IP address
#
###############################################################################

function configureNetwork() {
   # Check if running on Ubuntu
   if ! grep -q "ID=ubuntu" /etc/os-release; then
      echo "This script only supports Ubuntu. Current OS is not supported."
      exit 1
   fi

   # Check if all required environment variables are set
   if [ -z "${IF_NAME}" ] || [ -z "${IF_IP}" ] || [ -z "${IF_NETMASK}" ] || [ -z "${IF_GATEWAY}" ]; then
      echo "Error: Missing required environment variables."
      echo "Required variables: IF_NAME, IF_IP, IF_NETMASK, IF_GATEWAY"
      exit 1
   fi

   echo "nameserver 8.8.8.8" > /etc/resolv.conf

   cat>/etc/network/interfaces<<EOF
auto ${IF_NAME}
iface ${IF_NAME} inet static
address ${IF_IP}
netmask ${IF_NETMASK}
gateway ${IF_GATEWAY}
EOF

   /etc/init.d/networking restart
   if [ $? != 0 ];then
      echo "Network configuration failed!"
      exit 1
   fi
}

configureNetwork
