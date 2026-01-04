#!/bin/bash

# BeagleBone Target Settings - Enable Internet over USB
# This script automates the procedure to enable internet over USB on the target (BeagleBone)
# Run this script on the BeagleBone device with sudo privileges

set -e  # Exit on error

# Color codes for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print section headers
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Function to print success message
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to print warning message
print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Function to print error message
print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    print_error "This script must be run as root (use sudo)"
    exit 1
fi

print_header "BeagleBone USB Internet Setup - Target Configuration"

# Step 1: Configure IP forwarding in sysctl.conf
print_header "Step 1: Configuring IP forwarding in /etc/sysctl.conf"

if grep -q "^net.ipv4.ip_forward=1" /etc/sysctl.conf; then
    print_success "IP forwarding already enabled in sysctl.conf"
elif grep -q "^#net.ipv4.ip_forward=1" /etc/sysctl.conf; then
    # Uncomment the line
    sed -i 's/^#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf
    print_success "Uncommented net.ipv4.ip_forward=1 in sysctl.conf"
else
    # Add the line
    echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
    print_success "Added net.ipv4.ip_forward=1 to sysctl.conf"
fi

# Apply sysctl settings
sysctl -p > /dev/null 2>&1
print_success "Applied sysctl settings"

# Step 2: Load g_ether module
print_header "Step 2: Loading g_ether kernel module"

if lsmod | grep -q g_ether; then
    print_warning "g_ether module already loaded"
else
    modprobe g_ether
    print_success "Loaded g_ether module"
fi

# Wait for usb0 interface to be created
sleep 2

# Step 3: Configure usb0 interface
print_header "Step 3: Configuring usb0 interface"

if ip link show usb0 > /dev/null 2>&1; then
    ifconfig usb0 192.168.7.2 up
    print_success "Configured usb0 interface with IP 192.168.7.2"
else
    print_error "usb0 interface not found. Please check if g_ether loaded correctly"
    exit 1
fi

# Display network interfaces
echo ""
echo "Current network interfaces:"
ifconfig usb0 | grep -E "inet |usb0"
echo ""

# Step 4: Configure DNS resolvers
print_header "Step 4: Configuring DNS in /etc/resolv.conf"

# Backup original resolv.conf
if [ ! -f /etc/resolv.conf.backup ]; then
    cp /etc/resolv.conf /etc/resolv.conf.backup
    print_success "Backed up original resolv.conf"
fi

# Add Google DNS servers if not present
if ! grep -q "nameserver 8.8.8.8" /etc/resolv.conf; then
    echo "nameserver 8.8.8.8" >> /etc/resolv.conf
fi
if ! grep -q "nameserver 8.8.4.4" /etc/resolv.conf; then
    echo "nameserver 8.8.4.4" >> /etc/resolv.conf
fi
print_success "Configured DNS nameservers (8.8.8.8, 8.8.4.4)"

# Step 5: Configure network interfaces file
print_header "Step 5: Configuring /etc/network/interfaces"

# Backup original interfaces file
if [ ! -f /etc/network/interfaces.backup ]; then
    cp /etc/network/interfaces /etc/network/interfaces.backup
    print_success "Backed up original interfaces file"
fi

# Check if usb0 configuration already exists
if grep -q "iface usb0 inet static" /etc/network/interfaces; then
    print_warning "usb0 configuration already exists in /etc/network/interfaces"
else
    # Add usb0 configuration
    cat >> /etc/network/interfaces << 'EOF'

# Ethernet/RNDIS gadget (g_ether)
# Used by: /opt/scripts/boot/autoconfigure_usb0.sh
iface usb0 inet static
    address 192.168.7.2
    netmask 255.255.255.0
    network 192.168.7.0
    gateway 192.168.7.1
    dns-nameservers 8.8.8.8
    dns-nameservers 8.8.4.4
EOF
    print_success "Added usb0 configuration to /etc/network/interfaces"
fi

# Step 6: Add default gateway
print_header "Step 6: Adding default gateway"

# Remove existing default gateway to avoid conflicts
route del default 2>/dev/null || true

# Add new default gateway
route add default gw 192.168.7.1
print_success "Added default gateway 192.168.7.1"

# Display routing table
echo ""
echo "Current routing table:"
route -n
echo ""

# Step 7: Test connectivity
print_header "Step 7: Testing connectivity"

echo "Attempting to ping 8.8.8.8..."
if ping -c 3 8.8.8.8 > /dev/null 2>&1; then
    print_success "Successfully connected to internet!"
else
    print_warning "Could not reach 8.8.8.8. Please check host configuration."
    echo "Make sure the host computer has:"
    echo "  1. Run the usbnet.sh script to enable NAT"
    echo "  2. Configured proper IP forwarding"
fi

print_header "Configuration Complete"
echo ""
echo "Summary of configuration:"
echo "  - IP forwarding enabled in sysctl.conf"
echo "  - g_ether module loaded"
echo "  - usb0 interface configured (192.168.7.2)"
echo "  - DNS servers configured (8.8.8.8, 8.8.4.4)"
echo "  - Network interfaces file updated"
echo "  - Default gateway set (192.168.7.1)"
echo ""
print_success "BeagleBone is now configured for internet over USB"
echo ""
echo "Note: Configuration will persist after reboot."
echo "Backup files created: /etc/resolv.conf.backup, /etc/network/interfaces.backup"
