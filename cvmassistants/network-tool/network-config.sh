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
#   - ifName: Network interface name (e.g., eth0)
#   - ifIp: IP address to assign to the interface
#   - ifNetmask: Network subnet mask
#   - ifGateway: Gateway IP address
#
###############################################################################

function configureNetwork() {
   # Check if running on Ubuntu
   if ! grep -q "ID=ubuntu" /etc/os-release; then
      echo "This script only supports Ubuntu. Current OS is not supported."
      exit 1
   fi

   # Check if all required environment variables are set
   if [ -z "${ifName}" ] || [ -z "${ifIp}" ] || [ -z "${ifNetmask}" ] || [ -z "${ifGateway}" ]; then
      echo "Error: Missing required environment variables."
      echo "Required variables: ifName, ifIp, ifNetmask, ifGateway"
      exit 1
   fi

   echo "nameserver 8.8.8.8" > /etc/resolv.conf

   cat>/etc/network/interfaces<<EOF
auto ${ifName}
iface ${ifName} inet static
address ${ifIp}
netmask ${ifNetmask}
gateway ${ifGateway}
EOF

   /etc/init.d/networking restart
   if [ $? != 0 ];then
      echo "Network configuration failed!"
      exit 1
   fi
}

configureNetwork
