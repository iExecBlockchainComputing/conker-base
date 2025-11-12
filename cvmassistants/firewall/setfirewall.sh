#!/usr/bin/env bash
###############################################################################
# Script: setfirewall.sh
# Description: Configure UFW firewall rules on Ubuntu systems (e.g., TDX environment)
#
# This script enables UFW and allows ports defined in the environment variable
# `ALLOW_PORTS`. Supports single ports and port ranges (e.g., "22,80,3000:3010").
#
# Requirements:
#   - Must be run as root
#   - Must run on Ubuntu
#   - iptables and UFW must be installed
#
###############################################################################

log_info() {
  echo -e "[INFO] $*"
}

log_warn() {
  echo -e "[WARN] $*"
}

log_fatal() {
  echo -e "[ERROR] $*" >&2
  exit 1
}

log_info "Starting firewall configuration..."

# Check if running on Ubuntu
if ! grep -q "ID=ubuntu" /etc/os-release; then
    log_fatal "This script supports only Ubuntu. Aborting."
fi

# Load ip_tables module
log_info "Loading ip_tables module..."
modprobe ip_tables 2>/dev/null
if [ $? -ne 0 ]; then
    log_warn "Could not load ip_tables (module missing or already loaded)."
else
    log_info "ip_tables loaded successfully."
fi

# Enable UFW
log_info "Enabling UFW..."
ufw --force enable >/dev/null 2>&1
if [ $? -ne 0 ]; then
    log_fatal "Failed to enable UFW. Please ensure UFW is installed."
fi
log_info "UFW enabled."

# Get ports from environment variable
if [ -z "${ALLOW_PORTS}" ]; then
    log_info "No ports specified (ALLOW_PORTS is empty). Skipping rule creation."
else
    log_info "Allowing ports: ${ALLOW_PORTS}"
    IFS=',' read -ra PORT_ARRAY <<< "${ALLOW_PORTS}"

    for port in "${PORT_ARRAY[@]}"; do
        port="$(echo "$port" | xargs)"  # trim spaces
        if [[ "${port}" == *:* ]]; then
            start="${port%%:*}"
            end="${port##*:}"
            log_info "Allowing port range ${start}:${end}/tcp..."
            ufw allow "${start}:${end}/tcp" >/dev/null 2>&1
            if [ $? -eq 0 ]; then
                log_info "Range ${start}:${end}/tcp allowed."
            else
                log_warn "Failed to allow range ${start}:${end}/tcp."
            fi
        else
            log_info "Allowing port ${port}/tcp..."
            ufw allow "${port}/tcp" >/dev/null 2>&1
            if [ $? -eq 0 ]; then
                log_info "Port ${port}/tcp allowed."
            else
                log_warn "Failed to allow port ${port}/tcp."
            fi
        fi
    done
fi

log_info "Firewall configuration completed."
