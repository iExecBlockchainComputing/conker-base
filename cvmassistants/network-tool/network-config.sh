#!/usr/bin/env bash
BASEDIR="$( cd "$( dirname "$0"  )" && pwd  )"

function config() {
   echo "nameserver 8.8.8.8" > /etc/resolv.conf
   
   OS_TYPE=`cat /etc/os-release |grep -w ID |awk -F= '{print $2}'`

   case "$OS_TYPE" in
      ubuntu)
         cat>/etc/network/interfaces<<EOF
auto ${ifName}
iface ${ifName} inet static
address ${ifIp}
netmask ${ifNetmask}
gateway ${ifGateway}
EOF
      /etc/init.d/networking restart
      if [ $? != 0 ];then
         echo config network failed !
         exit -1
      fi
        ;;
      centos)
        echo not support now
        exit -1
        ;;
      alpine)
        echo not support now
        exit -1
        ;;
    *)
        echo unknow os $OS_TYPE exit!
        return
        ;;
     esac
}

function main() {
      config
}

main
