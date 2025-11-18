#!/bin/bash
################################################################################
# FreeSWITCH Installation Script for Debian 13 (Trixie)
# Brain-Core Integration Setup
################################################################################
#
# Bu script şunları yapar:
# 1. Sistem güncellemeleri ve temel build tools kurulumu
# 2. spandsp ve sofia-sip kaynak kodundan build ve kurulum
# 3. FreeSWITCH kaynak kodundan build ve kurulum
# 4. mod_event_socket ve Brain-Core iletişimi için gerekli modüllerin kurulumu
#
################################################################################

set -e  # Hata durumunda scripti durdur
set -u  # Tanımlanmamış değişken kullanımında hata ver

# Renkli output için
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Log fonksiyonları
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Konfigürasyon değişkenleri
WORK_DIR="/usr/local/src"
FREESWITCH_DIR="/usr/local/freeswitch"
BUILD_NUMBER="1"
DEBIAN_CODENAME="trixie"  # Debian 13

# Root kontrolü
if [ "$EUID" -ne 0 ]; then
    log_error "Bu script root olarak çalıştırılmalıdır. sudo ile çalıştırın."
    exit 1
fi

log_info "FreeSWITCH Kurulum Script'i başlatılıyor..."
log_info "Debian Codename: ${DEBIAN_CODENAME}"

################################################################################
# 1. SİSTEM GÜNCELLEMELERİ VE TEMEL PAKETLER
################################################################################

log_info "Sistem güncelleniyor..."
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get -y upgrade

log_info "Temel build tools kuruluyor..."
apt-get -y install \
    build-essential \
    git \
    wget \
    curl \
    pkg-config \
    cmake \
    automake \
    autoconf \
    libtool \
    libtool-bin \
    debhelper \
    devscripts \
    lsb-release \
    docbook-xsl \
    doxygen \
    yasm \
    bison

log_success "Temel paketler kuruldu"

################################################################################
# 2. FREESWITCH BUILD DEPENDENCIES
################################################################################

log_info "FreeSWITCH build dependencies kuruluyor..."

apt-get -y install \
    dpkg-dev \
    gcc \
    g++ \
    libc6-dev \
    make \
    libpcre2-dev \
    libedit-dev \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    libtiff5-dev \
    libncurses5-dev \
    unixodbc-dev \
    libpq-dev \
    libjpeg62-turbo-dev \
    python3-dev \
    python3-all-dev \
    dh-python \
    python3-setuptools \
    erlang-dev \
    uuid-dev \
    libexpat1-dev \
    libgdbm-dev \
    libdb-dev \
    libogg-dev \
    libspeex-dev \
    libspeexdsp-dev \
    libldns-dev

log_success "FreeSWITCH dependencies kuruldu"

################################################################################
# 3. SPANDSP KAYNAK KODUNDAN KURULUM
################################################################################

log_info "spandsp kuruluyor (kaynak kodundan)..."

cd "$WORK_DIR"

# Eğer mevcutsa eski klasörü temizle
if [ -d "spandsp" ]; then
    log_warning "Eski spandsp klasörü bulundu, temizleniyor..."
    rm -rf spandsp
fi

# spandsp repository'sini clone et
log_info "spandsp repository clone ediliyor..."
git clone --branch packages https://github.com/freeswitch/spandsp.git
cd spandsp

# Git güvenlik ayarı
git config --global --add safe.directory '*'

# Version bilgilerini al
export VERSION=$(dpkg-parsechangelog --show-field Version | cut -f1 -d'-')
export GIT_SHA=$(git rev-parse --short HEAD)

log_info "spandsp version: ${VERSION}-${BUILD_NUMBER}-${GIT_SHA}"

# Build dependencies'i kur
log_info "spandsp build dependencies kuruluyor..."
apt-get update
mk-build-deps --install --remove debian/control --tool "apt-get -y --no-install-recommends"
apt-get -y -f install

# Changelog güncelle
dch --controlmaint \
    --distribution "${DEBIAN_CODENAME}" \
    --force-bad-version \
    --force-distribution \
    --newversion "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${DEBIAN_CODENAME}" \
    "Custom build for Debian ${DEBIAN_CODENAME}"

# Package build et
log_info "spandsp build ediliyor..."
debuild --no-tgz-check --build=binary --unsigned-source --unsigned-changes

# Package'leri kur
log_info "spandsp packages kuruluyor..."
cd ..
dpkg -i libspandsp3_*.deb libspandsp3-dev_*.deb || apt-get -y -f install

log_success "spandsp başarıyla kuruldu"

################################################################################
# 4. SOFIA-SIP KAYNAK KODUNDAN KURULUM
################################################################################

log_info "sofia-sip kuruluyor (kaynak kodundan)..."

cd "$WORK_DIR"

# Eğer mevcutsa eski klasörü temizle
if [ -d "sofia-sip" ]; then
    log_warning "Eski sofia-sip klasörü bulundu, temizleniyor..."
    rm -rf sofia-sip
fi

# sofia-sip repository'sini clone et
log_info "sofia-sip repository clone ediliyor..."
git clone https://github.com/freeswitch/sofia-sip.git
cd sofia-sip

# Version bilgilerini al
export VERSION=$(dpkg-parsechangelog --show-field Version | cut -f1 -d'-')
export GIT_SHA=$(git rev-parse --short HEAD)

log_info "sofia-sip version: ${VERSION}-${BUILD_NUMBER}-${GIT_SHA}"

# Build dependencies'i kur
log_info "sofia-sip build dependencies kuruluyor..."
apt-get update
mk-build-deps --install --remove debian/control --tool "apt-get -y --no-install-recommends"
apt-get -y -f install

# Changelog güncelle
dch --controlmaint \
    --distribution "${DEBIAN_CODENAME}" \
    --force-bad-version \
    --force-distribution \
    --newversion "${VERSION}-${BUILD_NUMBER}-${GIT_SHA}~${DEBIAN_CODENAME}" \
    "Custom build for Debian ${DEBIAN_CODENAME}"

# Package build et
log_info "sofia-sip build ediliyor..."
debuild --no-tgz-check --build=binary --unsigned-source --unsigned-changes

# Package'leri kur
log_info "sofia-sip packages kuruluyor..."
cd ..
dpkg -i libsofia-sip-ua0_*.deb libsofia-sip-ua-dev_*.deb || apt-get -y -f install

log_success "sofia-sip başarıyla kuruldu"

################################################################################
# 5. FREESWITCH KAYNAK KODUNDAN BUILD VE KURULUM
################################################################################

log_info "FreeSWITCH build ediliyor..."

# FreeSWITCH source directory'ye geç
cd /home/user/freeswitch

# Bootstrap - autoconf/automake scriptlerini oluştur
log_info "FreeSWITCH bootstrap yapılıyor..."
./bootstrap.sh -j

# modules.conf dosyasını ayarla (Brain-Core için gerekli modüller)
log_info "modules.conf yapılandırılıyor..."
cat > modules.conf << 'EOF'
# Core Applications
applications/mod_commands
applications/mod_conference
applications/mod_db
applications/mod_dptools
applications/mod_hash
applications/mod_fifo
applications/mod_voicemail

# Dialplan
dialplans/mod_dialplan_xml

# Endpoints - SIP için kritik
endpoints/mod_sofia
endpoints/mod_loopback

# Event Handlers - Brain-Core iletişimi için KRİTİK!
event_handlers/mod_event_socket
event_handlers/mod_cdr_csv

# Formats
formats/mod_local_stream
formats/mod_native_file
formats/mod_sndfile
formats/mod_tone_stream

# Languages
languages/mod_lua

# Loggers
loggers/mod_console
loggers/mod_logfile
loggers/mod_syslog

# Say
say/mod_say_en

# Timers
timers/mod_posix_timer

# Codecs
codecs/mod_g723_1
codecs/mod_g729
codecs/mod_amr
codecs/mod_opus

# Databases
databases/mod_pgsql

# XML
xml_int/mod_xml_cdr
xml_int/mod_xml_curl

# SpanDSP - Fax support
applications/mod_spandsp
EOF

# Configure - build seçeneklerini ayarla
log_info "FreeSWITCH configure yapılıyor..."
./configure \
    --prefix=${FREESWITCH_DIR} \
    --enable-core-pgsql-support \
    --enable-static-v8 \
    --disable-debug

# Make - compile et (çok çekirdekli build için -j parametresi)
log_info "FreeSWITCH compile ediliyor (bu uzun sürebilir, lütfen bekleyin)..."
CPU_CORES=$(nproc)
make -j${CPU_CORES}

# Install
log_info "FreeSWITCH kuruluyor ${FREESWITCH_DIR} dizinine..."
make install

# Sounds ve config dosyalarını kur
log_info "FreeSWITCH sounds ve config dosyaları kuruluyor..."
make cd-sounds-install cd-moh-install

# Sample config'leri kopyala
log_info "Sample config dosyaları kopyalanıyor..."
cp -a /home/user/freeswitch/conf/vanilla ${FREESWITCH_DIR}/conf/

log_success "FreeSWITCH başarıyla kuruldu: ${FREESWITCH_DIR}"

################################################################################
# 6. FREESWITCH KULLANICI VE İZİNLER
################################################################################

log_info "FreeSWITCH kullanıcı ve grup oluşturuluyor..."

# FreeSWITCH kullanıcı ve grup oluştur
if ! getent group freeswitch > /dev/null; then
    groupadd -r freeswitch
    log_success "freeswitch grubu oluşturuldu"
fi

if ! getent passwd freeswitch > /dev/null; then
    useradd -r -g freeswitch -d ${FREESWITCH_DIR} -s /bin/false -c "FreeSWITCH" freeswitch
    log_success "freeswitch kullanıcısı oluşturuldu"
fi

# Dizin sahipliklerini ayarla
log_info "Dizin izinleri ayarlanıyor..."
chown -R freeswitch:freeswitch ${FREESWITCH_DIR}
chmod -R u=rwX,g=rX,o= ${FREESWITCH_DIR}

################################################################################
# 7. SYSTEMD SERVICE AYARLARI
################################################################################

log_info "Systemd service dosyası oluşturuluyor..."

cat > /etc/systemd/system/freeswitch.service << EOF
[Unit]
Description=FreeSWITCH
After=network.target postgresql.service

[Service]
Type=forking
PIDFile=/usr/local/freeswitch/run/freeswitch.pid
Environment="DAEMON_OPTS=-nonat"
EnvironmentFile=-/etc/default/freeswitch
ExecStart=/usr/local/freeswitch/bin/freeswitch -u freeswitch -g freeswitch -ncwait \$DAEMON_OPTS
TimeoutSec=45s
Restart=on-failure
RestartSec=10s
LimitNOFILE=1000000
LimitNPROC=60000
LimitSTACK=250000
LimitRTPRIO=infinity
LimitRTTIME=infinity
IOSchedulingClass=realtime
IOSchedulingPriority=2
CPUSchedulingPolicy=rr
CPUSchedulingPriority=89

[Install]
WantedBy=multi-user.target
EOF

# Systemd reload
systemctl daemon-reload

log_success "Systemd service dosyası oluşturuldu"

################################################################################
# 8. ESL (Event Socket Library) PYTHON BINDING KURULUMU
################################################################################

log_info "ESL Python binding kuruluyor (Brain-Core için)..."

cd /home/user/freeswitch/libs/esl
make pymod
make pymod-install

# Python ESL modülünü system path'e kopyala
PYTHON_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
PYTHON_SITE_PACKAGES="/usr/local/lib/python${PYTHON_VERSION}/dist-packages"

mkdir -p ${PYTHON_SITE_PACKAGES}
cp -v /home/user/freeswitch/libs/esl/python/ESL.py ${PYTHON_SITE_PACKAGES}/
cp -v /home/user/freeswitch/libs/esl/python/_ESL.so ${PYTHON_SITE_PACKAGES}/

log_success "ESL Python binding kuruldu"

################################################################################
# 9. ÖZET BİLGİLER
################################################################################

log_success "======================================================================"
log_success "FreeSWITCH Kurulumu Tamamlandı!"
log_success "======================================================================"
echo ""
log_info "Kurulum Dizini: ${FREESWITCH_DIR}"
log_info "Config Dizini: ${FREESWITCH_DIR}/conf/"
log_info "FreeSWITCH Binary: ${FREESWITCH_DIR}/bin/freeswitch"
log_info "FreeSWITCH CLI: ${FREESWITCH_DIR}/bin/fs_cli"
echo ""
log_info "Kurulu Kritik Modüller:"
log_info "  - mod_sofia (SIP Stack)"
log_info "  - mod_event_socket (Brain-Core iletişimi için)"
log_info "  - mod_spandsp (DSP ve Codec)"
echo ""
log_info "Systemd Komutları:"
log_info "  Başlat:  systemctl start freeswitch"
log_info "  Durdur:  systemctl stop freeswitch"
log_info "  Durum:   systemctl status freeswitch"
log_info "  Boot'ta başlat: systemctl enable freeswitch"
echo ""
log_info "FreeSWITCH CLI'ya bağlanmak için:"
log_info "  ${FREESWITCH_DIR}/bin/fs_cli"
echo ""
log_warning "ÖNEMLİ: FreeSWITCH'i başlatmadan önce:"
log_warning "  1. ${FREESWITCH_DIR}/conf/ dizinindeki config dosyalarını düzenleyin"
log_warning "  2. SIP profili ayarlarını NetGSM için yapılandırın"
log_warning "  3. Event Socket ayarlarını Brain-Core için yapılandırın"
echo ""
log_info "Event Socket Ayarları (Brain-Core için):"
log_info "  Config: ${FREESWITCH_DIR}/conf/autoload_configs/event_socket.conf.xml"
log_info "  Default Port: 8021"
log_info "  Default Password: ClueCon"
echo ""
log_success "======================================================================"
