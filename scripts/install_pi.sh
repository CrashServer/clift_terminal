#!/bin/bash
# CRIC Installation Engine — Raspberry Pi Setup
# Run on a fresh Raspberry Pi OS Lite installation.
# Usage: sudo ./install_pi.sh
#
# Prerequisites: Raspberry Pi OS Lite (64-bit or 32-bit), network access

set -euo pipefail

CRIC_USER="${CRIC_USER:-pi}"
CRIC_DIR="/home/${CRIC_USER}/cric"
CRIC_REPO="${CRIC_REPO:-}"  # Optional: git clone URL

log() {
    echo "[CRIC SETUP] $*"
}

if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo: sudo $0"
    exit 1
fi

log "Installing build dependencies..."
apt-get update
apt-get install -y \
    build-essential \
    libncurses5-dev libncursesw5-dev \
    libssl-dev \
    curl jq python3 \
    sqlite3

# Optional PipeWire (not usually on Pi Lite, but if available)
apt-get install -y libpipewire-0.3-dev libspa-0.2-dev 2>/dev/null || true

log "Configuring console font for projector output..."
# Set a large, readable console font
# For 1080p HDMI: 16x32 gives ~60x33 chars
# For 720p: 12x24 gives ~80x30 chars
cat > /etc/default/console-setup << 'EOF'
ACTIVE_CONSOLES="/dev/tty[1-6]"
CHARMAP="UTF-8"
CODESET="Uni3"
FONTFACE="Terminus"
FONTSIZE="16x32"
EOF
dpkg-reconfigure -f noninteractive console-setup

log "Configuring auto-login on tty1..."
mkdir -p /etc/systemd/system/getty@tty1.service.d
cat > /etc/systemd/system/getty@tty1.service.d/autologin.conf << EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin ${CRIC_USER} --noclear %I \$TERM
EOF

log "Setting up CRIC directory..."
if [ -n "$CRIC_REPO" ]; then
    sudo -u "$CRIC_USER" git clone "$CRIC_REPO" "$CRIC_DIR" 2>/dev/null || true
fi

if [ ! -d "$CRIC_DIR" ]; then
    log "WARNING: $CRIC_DIR does not exist. Copy the CRIC repository there manually."
    mkdir -p "$CRIC_DIR"
    chown "$CRIC_USER:$CRIC_USER" "$CRIC_DIR"
fi

log "Building clift..."
if [ -d "${CRIC_DIR}/clift_terminal/scripts" ]; then
    cd "${CRIC_DIR}/clift_terminal/scripts"
    sudo -u "$CRIC_USER" make clean
    sudo -u "$CRIC_USER" make
    log "Build successful"
else
    log "WARNING: clift_terminal/scripts not found, skipping build"
fi

log "Installing systemd services..."

# Main installation service
cat > /etc/systemd/system/cric-installation.service << EOF
[Unit]
Description=CRIC Installation Engine
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${CRIC_USER}
WorkingDirectory=${CRIC_DIR}/clift_terminal
ExecStart=${CRIC_DIR}/clift_terminal/clift --installation
Restart=always
RestartSec=3
StandardInput=tty
StandardOutput=tty
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes

[Install]
WantedBy=multi-user.target
EOF

# Feed fetcher service
cat > /etc/systemd/system/cric-feeds.service << EOF
[Unit]
Description=CRIC Feed Fetcher
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${CRIC_USER}
WorkingDirectory=${CRIC_DIR}/clift_terminal
ExecStart=${CRIC_DIR}/clift_terminal/scripts/feed_fetcher.sh
Restart=always
RestartSec=10
Environment=MASTODON_INSTANCE=mastodon.social
Environment=MASTODON_KEYWORDS=cyberpunk,technology,resistance,surveillance,hacking

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable cric-installation.service
systemctl enable cric-feeds.service

log "Configuring HDMI output..."
# Force HDMI output on (Pi doesn't always detect projectors)
if [ -f /boot/config.txt ]; then
    # Pi 3/4 config
    if ! grep -q "hdmi_force_hotplug=1" /boot/config.txt; then
        cat >> /boot/config.txt << 'EOF'

# CRIC Installation: Force HDMI
hdmi_force_hotplug=1
hdmi_group=1
hdmi_mode=16
disable_overscan=1
EOF
    fi
elif [ -f /boot/firmware/config.txt ]; then
    # Pi 5 / newer OS
    if ! grep -q "hdmi_force_hotplug=1" /boot/firmware/config.txt; then
        cat >> /boot/firmware/config.txt << 'EOF'

# CRIC Installation: Force HDMI
hdmi_force_hotplug=1
hdmi_group=1
hdmi_mode=16
disable_overscan=1
EOF
    fi
fi

log "Disabling screen blanking..."
# Prevent console blanking
if ! grep -q "consoleblank=0" /boot/cmdline.txt 2>/dev/null; then
    sed -i 's/$/ consoleblank=0/' /boot/cmdline.txt 2>/dev/null || true
fi
if ! grep -q "consoleblank=0" /boot/firmware/cmdline.txt 2>/dev/null; then
    sed -i 's/$/ consoleblank=0/' /boot/firmware/cmdline.txt 2>/dev/null || true
fi

log ""
log "============================================"
log "  CRIC Installation Setup Complete"
log "============================================"
log ""
log "  Services installed:"
log "    - cric-installation.service (main engine)"
log "    - cric-feeds.service (data fetcher)"
log ""
log "  To start now:"
log "    sudo systemctl start cric-feeds"
log "    sudo systemctl start cric-installation"
log ""
log "  To test without systemd:"
log "    cd ${CRIC_DIR}/clift_terminal"
log "    ./clift --installation"
log ""
log "  Reboot to auto-start on tty1."
log ""
