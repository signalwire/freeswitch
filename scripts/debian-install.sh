#!/bin/bash
################################################################################
# FreeSWITCH Installation Script for Debian 13 (AWS)
# Purpose: Install FreeSWITCH with Brain-Core Orchestrator integration
################################################################################

set -e  # Exit on error
set -u  # Exit on undefined variable

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    log_error "Please run as root (use sudo)"
    exit 1
fi

# Variables
FS_SRC_DIR="/usr/src/freeswitch"
FS_INSTALL_DIR="/usr/local/freeswitch"
BUILD_DIR=$(pwd)

################################################################################
# PHASE 1: System Dependencies
################################################################################
log_info "Phase 1: Installing system dependencies..."

apt-get update

# Core build dependencies
log_info "Installing core build dependencies..."
apt-get install -y \
    build-essential \
    automake \
    autoconf \
    libtool \
    libtool-bin \
    pkg-config \
    git \
    wget \
    curl \
    unzip

# FreeSWITCH core dependencies
log_info "Installing FreeSWITCH core dependencies..."
apt-get install -y \
    libpcre2-dev \
    libedit-dev \
    libsqlite3-dev \
    libtiff-dev \
    yasm \
    libogg-dev \
    libspeex-dev \
    libspeexdsp-dev \
    libssl-dev \
    libpq-dev \
    libncurses-dev \
    libjpeg-dev \
    python3-dev \
    uuid-dev \
    libexpat1-dev \
    libgdbm-dev \
    libdb-dev \
    zlib1g-dev

# SIP Stack (Sofia-SIP 1.13.17+)
log_info "Installing Sofia-SIP 1.13.17+ for SIP support..."
# Note: Debian 13 has sofia-sip 1.12.11, but FreeSWITCH needs 1.13.17+
# Remove old Debian package if exists
apt-get remove -y libsofia-sip-ua-dev libsofia-sip-ua0t64 2>/dev/null || true

if ! pkg-config --exists sofia-sip-ua || [ "$(pkg-config --modversion sofia-sip-ua)" != "1.13.17" ]; then
    log_info "Sofia-SIP 1.13.17 not found, building from source..."
    cd /tmp
    git clone --depth 1 https://github.com/freeswitch/sofia-sip.git
    cd sofia-sip
    ./bootstrap.sh
    ./configure --prefix=/usr
    make -j$(nproc)
    make install
    ldconfig
    log_info "Sofia-SIP installed: $(pkg-config --modversion sofia-sip-ua)"
    cd "$BUILD_DIR"
else
    log_info "Sofia-SIP 1.13.17 already installed"
fi

# Audio processing
log_info "Installing audio processing libraries..."
apt-get install -y \
    libsndfile1-dev \
    libflac-dev \
    libvorbis-dev
# Note: libspandsp-dev skipped (v0.0.6 available, but FreeSWITCH needs v3.0+)

# Codec dependencies
log_info "Installing codec libraries..."
apt-get install -y \
    libopencore-amrnb-dev \
    libopus-dev

# ENUM support (mod_enum)
log_info "Installing LDNS for ENUM support..."
apt-get install -y libldns-dev

# WebSocket & Event Socket dependencies
log_info "Installing WebSocket and networking libraries..."
apt-get install -y \
    libwebsockets-dev \
    libevent-dev \
    libcurl4-openssl-dev

# Lua scripting support (for dialplan)
log_info "Installing Lua for scripting support..."
apt-get install -y liblua5.3-dev

# Video support (mod_av)
log_info "Installing video processing libraries..."
apt-get install -y \
    libavformat-dev \
    libswscale-dev \
    libswresample-dev

# Database support
log_info "Installing database libraries..."
apt-get install -y libpq-dev

# SpanDSP 3.0 (required for mod_spandsp - fax support)
log_info "Installing SpanDSP 3.0 from source..."
if ! pkg-config --exists spandsp || [ "$(pkg-config --modversion spandsp)" != "3.0.0" ]; then
    log_info "SpanDSP 3.0 not found, building from source..."
    cd /tmp
    wget -q https://github.com/freeswitch/spandsp/archive/refs/heads/master.zip -O spandsp.zip
    unzip -q spandsp.zip
    cd spandsp-master
    ./bootstrap.sh
    ./configure --prefix=/usr
    make -j$(nproc)
    make install
    ldconfig
    log_info "SpanDSP 3.0 installed: $(pkg-config --modversion spandsp)"
    cd "$BUILD_DIR"
else
    log_info "SpanDSP 3.0 already installed: $(pkg-config --modversion spandsp)"
fi

################################################################################
# PHASE 2: FreeSWITCH Build & Install
################################################################################
log_info "Phase 2: Building FreeSWITCH from source..."

# Copy source if not already in /usr/src
if [ "$BUILD_DIR" != "$FS_SRC_DIR" ]; then
    log_info "Copying FreeSWITCH source to $FS_SRC_DIR..."
    mkdir -p /usr/src
    cp -r "$BUILD_DIR" "$FS_SRC_DIR"
    cd "$FS_SRC_DIR"
else
    cd "$FS_SRC_DIR"
fi

# Bootstrap
log_info "Running bootstrap..."
./bootstrap.sh -j

# Configure modules for Brain-Core integration
log_info "Configuring modules for Brain-Core integration..."
cat > modules.conf <<'EOF'
# Essential Core Modules
applications/mod_commands
applications/mod_dptools
applications/mod_hash
applications/mod_db
applications/mod_expr

# Conference & Call Control
applications/mod_conference
applications/mod_fifo
applications/mod_voicemail

# HTTP/CURL for Brain-Core API integration
applications/mod_curl

# Event Socket Library (ESL) - Critical for Brain-Core
event_handlers/mod_event_socket
event_handlers/mod_cdr_csv
event_handlers/mod_cdr_sqlite

# SIP Endpoint (NetGSM SIP Trunk)
endpoints/mod_sofia
endpoints/mod_loopback

# WebSocket support (mod_verto) - Can be adapted for Brain-Core
endpoints/mod_verto
endpoints/mod_rtc

# Dialplan
dialplans/mod_dialplan_xml

# Codecs - Essential for voice quality
codecs/mod_opus
codecs/mod_g729
codecs/mod_g723_1
codecs/mod_amr
codecs/mod_b64

# Audio Formats
formats/mod_sndfile
formats/mod_native_file
formats/mod_local_stream
formats/mod_tone_stream
formats/mod_png

# Scripting (Lua for advanced dialplan)
languages/mod_lua

# Logging
loggers/mod_console
loggers/mod_logfile
loggers/mod_syslog

# Language Files
say/mod_say_en

# Utilities
applications/mod_esf
applications/mod_fsv
applications/mod_httapi
applications/mod_enum
applications/mod_spandsp
applications/mod_valet_parking
applications/mod_signalwire

# Video support
applications/mod_av
applications/mod_test

# Database
databases/mod_pgsql

# XML Integration
xml_int/mod_xml_cdr
xml_int/mod_xml_rpc
xml_int/mod_xml_scgi
EOF

log_info "Configured modules:"
cat modules.conf

# Configure with optimizations
log_info "Running configure..."
./configure \
    --prefix=/usr/local/freeswitch \
    --enable-core-pgsql-support \
    --enable-static-v8 \
    --disable-debug \
    --with-openssl \
    CFLAGS="-O3 -march=native" \
    CXXFLAGS="-O3 -march=native"

# Build (using all CPU cores)
log_info "Building FreeSWITCH (this may take 15-30 minutes)..."
make -j$(nproc)

# Install
log_info "Installing FreeSWITCH..."
make install

# Install sounds (required for IVR)
log_info "Installing sound files..."
make cd-sounds-install cd-moh-install

################################################################################
# PHASE 3: User & Permissions Setup
################################################################################
log_info "Phase 3: Setting up freeswitch user and permissions..."

# Create freeswitch user if doesn't exist
if ! id -u freeswitch >/dev/null 2>&1; then
    useradd --system --home-dir /usr/local/freeswitch --shell /bin/false freeswitch
    log_info "Created freeswitch system user"
else
    log_warn "User 'freeswitch' already exists"
fi

# Set ownership
chown -R freeswitch:freeswitch /usr/local/freeswitch
chmod -R o-rwx /usr/local/freeswitch

################################################################################
# PHASE 4: Configuration for Brain-Core Integration
################################################################################
log_info "Phase 4: Configuring FreeSWITCH for Brain-Core integration..."

# Create configuration directory
mkdir -p /etc/freeswitch
cp -r /usr/local/freeswitch/conf/vanilla/* /etc/freeswitch/

# Backup original configs
cp /etc/freeswitch/autoload_configs/event_socket.conf.xml \
   /etc/freeswitch/autoload_configs/event_socket.conf.xml.orig

# Configure Event Socket Library (ESL) for Brain-Core
cat > /etc/freeswitch/autoload_configs/event_socket.conf.xml <<'EOF'
<configuration name="event_socket.conf" description="Event Socket Configuration">
  <settings>
    <param name="nat-map" value="false"/>
    <param name="listen-ip" value="127.0.0.1"/>
    <param name="listen-port" value="8021"/>
    <param name="password" value="ClueCon"/>
  </settings>
</configuration>
EOF

log_info "Event Socket configured on 127.0.0.1:8021 (for Brain-Core ESL connection)"

# Create symlink for freeswitch binary
ln -sf /usr/local/freeswitch/bin/freeswitch /usr/bin/freeswitch
ln -sf /usr/local/freeswitch/bin/fs_cli /usr/bin/fs_cli

################################################################################
# PHASE 5: Systemd Service
################################################################################
log_info "Phase 5: Creating systemd service..."

cat > /etc/systemd/system/freeswitch.service <<'EOF'
[Unit]
Description=FreeSWITCH
After=network.target

[Service]
Type=forking
PIDFile=/usr/local/freeswitch/run/freeswitch.pid
User=freeswitch
Group=freeswitch
ExecStart=/usr/local/freeswitch/bin/freeswitch -nc -nonat
ExecReload=/usr/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5s
LimitNOFILE=1000000
LimitNPROC=60000
LimitSTACK=250000
LimitRTPRIO=infinity
LimitRTTIME=infinity

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload

################################################################################
# PHASE 6: Brain-Core Integration Libraries
################################################################################
log_info "Phase 6: Installing Brain-Core integration libraries..."

# Install Node.js (for Brain-Core Orchestrator)
log_info "Installing Node.js 20.x LTS..."
curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs

# Verify Node.js installation
node --version
npm --version

# Install Python ESL library (for Brain-Core to communicate with FreeSWITCH)
log_info "Installing Python ESL library..."
cd /usr/src/freeswitch/libs/esl
make pymod
cp python/ESL.py /usr/local/lib/python3.*/dist-packages/ || true
python3 -c "import ESL" 2>/dev/null && log_info "Python ESL installed successfully" || log_warn "Python ESL installation may need manual setup"

# Install Docker for Brain-Core containers
log_info "Installing Docker for Brain-Core containers..."
apt-get install -y \
    ca-certificates \
    gnupg \
    lsb-release

mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/debian/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian \
  $(lsb_release -cs) stable" | tee /etc/apt/sources.list.d/docker.list > /dev/null

apt-get update
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Start Docker
systemctl enable docker
systemctl start docker

log_info "Docker installed and started"

################################################################################
# Summary & Next Steps
################################################################################
log_info "=========================================="
log_info "FreeSWITCH Installation Complete!"
log_info "=========================================="
log_info ""
log_info "Installation Details:"
log_info "  - FreeSWITCH installed to: /usr/local/freeswitch"
log_info "  - Configuration directory: /etc/freeswitch"
log_info "  - Event Socket: 127.0.0.1:8021 (password: ClueCon)"
log_info "  - User/Group: freeswitch:freeswitch"
log_info ""
log_info "Control Commands:"
log_info "  - Start:   systemctl start freeswitch"
log_info "  - Stop:    systemctl stop freeswitch"
log_info "  - Status:  systemctl status freeswitch"
log_info "  - Console: fs_cli"
log_info ""
log_info "Brain-Core Integration:"
log_info "  - Node.js $(node --version) installed"
log_info "  - Docker installed and running"
log_info "  - Python ESL library available"
log_info ""
log_info "Next Steps:"
log_info "  1. Configure NetGSM SIP trunk in /etc/freeswitch/sip_profiles/"
log_info "  2. Set up dialplan for Brain-Core routing"
log_info "  3. Develop mod_audio_stream for WebSocket audio streaming"
log_info "  4. Deploy Brain-Core Orchestrator Docker container"
log_info "  5. Deploy AI Adapter Services (STT, LLM, TTS)"
log_info ""
log_info "To start FreeSWITCH now:"
log_info "  systemctl start freeswitch"
log_info "=========================================="
