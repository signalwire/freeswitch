# FreeSWITCH Debian 13 Kurulum Rehberi

## 🎯 Hızlı Başlangıç

```bash
# 1. Root olarak kurulum scriptini çalıştır
sudo ./install-debian13.sh

# 2. Kurulum tamamlandıktan sonra FreeSWITCH'i başlat
sudo systemctl start freeswitch

# 3. CLI'ya bağlan ve test et
/usr/local/freeswitch/bin/fs_cli
```

## 📋 Gereksinimler

- **İşletim Sistemi:** Debian 13 (Trixie) - AWS EC2 Instance
- **Minimum RAM:** 2GB (4GB önerilir)
- **Disk Alanı:** 5GB boş alan
- **Root Erişimi:** Kurulum için gerekli
- **İnternet Bağlantısı:** Package download için gerekli

## 📦 Kurulum Dosyaları

### Ana Kurulum Scripti
- **`install-debian13.sh`** - Otomatik kurulum scripti (30-60 dakika)

### Dokümantasyon
- **`brain-core-integration-guide.md`** - Brain-Core entegrasyon rehberi
- **`SETUP-DEBIAN13.md`** - Bu dosya

## 🔧 Script'in Yaptığı İşlemler

### 1. Sistem Hazırlığı
- ✅ Sistem güncelleme (`apt-get update && upgrade`)
- ✅ Build tools kurulumu (gcc, g++, make, cmake, vb.)
- ✅ FreeSWITCH dependencies kurulumu (100+ paket)

### 2. Kaynak Kodundan Build
- ✅ **spandsp** - DSP library (kaynak kodundan)
- ✅ **sofia-sip** - SIP stack (kaynak kodundan)
- ✅ **FreeSWITCH** - Main application (kaynak kodundan)

### 3. Modüller
Aşağıdaki kritik modüller kurulur:
- ✅ `mod_sofia` - SIP endpoint (NetGSM trunk için)
- ✅ `mod_event_socket` - ESL (Brain-Core iletişimi için)
- ✅ `mod_spandsp` - DSP ve codec support
- ✅ `mod_lua` - Scripting
- ✅ `mod_dptools` - Dialplan tools
- ✅ Ve daha fazlası...

### 4. Sistem Entegrasyonu
- ✅ FreeSWITCH kullanıcı ve grup oluşturma
- ✅ Systemd service ayarları
- ✅ ESL Python binding kurulumu
- ✅ Directory permissions

## 🚀 Adım Adım Kurulum

### Adım 1: Repository Clone (Zaten yapılmış)

```bash
# Bu adım zaten tamamlanmış durumda
cd /home/user/freeswitch
```

### Adım 2: Script İzinlerini Ayarla

```bash
chmod +x install-debian13.sh
```

### Adım 3: Kurulumu Başlat

```bash
# Root olarak çalıştırın
sudo ./install-debian13.sh
```

**Not:** İlk kurulum 30-60 dakika sürebilir. Script aşağıdaki adımları gerçekleştirir:
1. Paket güncellemeleri (5-10 dk)
2. spandsp build (5-10 dk)
3. sofia-sip build (5-10 dk)
4. FreeSWITCH build (15-30 dk)

### Adım 4: Kurulum Doğrulama

```bash
# FreeSWITCH version kontrolü
/usr/local/freeswitch/bin/freeswitch -version

# Binary'lerin varlığını kontrol et
ls -la /usr/local/freeswitch/bin/

# Modülleri kontrol et
ls -la /usr/local/freeswitch/mod/
```

## ⚙️ Konfigürasyon (Kurulum Sonrası)

### 1. Event Socket Ayarları

Brain-Core ile iletişim için gerekli:

```bash
# Config dosyasını düzenle
sudo nano /usr/local/freeswitch/conf/autoload_configs/event_socket.conf.xml
```

**Önemli Parametreler:**
- `listen-ip`: `127.0.0.1` (localhost - güvenlik için)
- `listen-port`: `8021` (default ESL port)
- `password`: **Güçlü bir şifre belirleyin!**

### 2. NetGSM SIP Trunk Ayarları

```bash
# External profile için gateway tanımla
sudo nano /usr/local/freeswitch/conf/sip_profiles/external/netgsm.xml
```

Örnek konfigürasyon `brain-core-integration-guide.md` dosyasında.

### 3. Dialplan Ayarları

```bash
# Gelen aramaları Brain-Core'a yönlendir
sudo nano /usr/local/freeswitch/conf/dialplan/default/01_brain_core.xml
```

## 🏃 FreeSWITCH'i Çalıştırma

### Systemd ile Başlat (Önerilen)

```bash
# Başlat
sudo systemctl start freeswitch

# Durum kontrol
sudo systemctl status freeswitch

# Boot'ta otomatik başlat
sudo systemctl enable freeswitch

# Durdur
sudo systemctl stop freeswitch

# Restart
sudo systemctl restart freeswitch
```

### Manuel Başlatma (Debug için)

```bash
# Foreground'da çalıştır (debug için)
sudo /usr/local/freeswitch/bin/freeswitch -nonat -nf

# Veya background'da
sudo /usr/local/freeswitch/bin/freeswitch -nonat
```

### CLI (Command Line Interface)

```bash
# CLI'ya bağlan
/usr/local/freeswitch/bin/fs_cli

# Veya şifre ile
/usr/local/freeswitch/bin/fs_cli -p ClueCon

# CLI içinde kullanılabilecek komutlar:
fs_cli> status                    # Sistem durumu
fs_cli> sofia status              # SIP profilleri
fs_cli> show channels             # Aktif kanallar
fs_cli> show calls                # Aktif aramalar
fs_cli> reloadxml                 # Config yenile
fs_cli> fsctl loglevel DEBUG      # Log level ayarla
fs_cli> /quit                     # CLI'dan çık
```

## 🐛 Sorun Giderme

### FreeSWITCH Başlamıyor

```bash
# Log dosyasını kontrol et
sudo tail -f /usr/local/freeswitch/log/freeswitch.log

# Manuel başlatarak hataları gör
sudo /usr/local/freeswitch/bin/freeswitch -nonat -nf

# Permissions kontrolü
sudo ls -la /usr/local/freeswitch/
```

### ESL Bağlantı Problemi

```bash
# Port dinliyor mu?
sudo netstat -tlnp | grep 8021

# Firewall kontrolü
sudo ufw status

# ESL config kontrolü
cat /usr/local/freeswitch/conf/autoload_configs/event_socket.conf.xml
```

### Build Hataları

```bash
# Dependencies eksik olabilir
sudo apt-get -f install

# Build klasörünü temizle ve tekrar dene
cd /home/user/freeswitch
make clean
./configure --prefix=/usr/local/freeswitch
make
sudo make install
```

### SIP Registration Problemi (NetGSM)

```bash
# CLI'dan gateway durumunu kontrol et
fs_cli> sofia status gateway netgsm

# Profile restart
fs_cli> sofia profile external restart

# SIP trace (debug)
fs_cli> sofia global siptrace on
```

## 📊 Sistem Gereksinimleri ve Performans

### Minimum Sistem
- **CPU:** 2 cores
- **RAM:** 2GB
- **Disk:** 5GB

### Önerilen Sistem (Production)
- **CPU:** 4+ cores
- **RAM:** 4GB+
- **Disk:** 20GB SSD
- **Network:** 100 Mbps+

### Eşzamanlı Kanal Kapasitesi
- **2 CPU / 2GB RAM:** ~50 eşzamanlı kanal
- **4 CPU / 4GB RAM:** ~100 eşzamanlı kanal
- **8 CPU / 8GB RAM:** ~200+ eşzamanlı kanal

## 🔒 Güvenlik Kontrol Listesi

- [ ] Event Socket şifresini değiştir
- [ ] Firewall kurallarını ayarla (sadece gerekli portlar)
- [ ] FreeSWITCH kullanıcısı ile çalıştır (root DEĞİL)
- [ ] SIP şifrelerini güçlü yap
- [ ] Log dosyalarını düzenli sil/rotate et
- [ ] ACL (Access Control List) ayarla
- [ ] TLS/SSL kullan (production'da)

## 📁 Önemli Dizinler

```
/usr/local/freeswitch/
├── bin/                    # Binary dosyalar (freeswitch, fs_cli)
├── conf/                   # Konfigürasyon dosyaları
│   ├── autoload_configs/   # Modül konfigürasyonları
│   ├── dialplan/           # Dialplan XML'leri
│   └── sip_profiles/       # SIP profilleri
├── mod/                    # FreeSWITCH modülleri (.so files)
├── log/                    # Log dosyaları
├── db/                     # SQLite database
├── recordings/             # Kayıtlar
└── sounds/                 # Ses dosyaları
```

## 🔄 Güncelleme

FreeSWITCH güncellemek için:

```bash
cd /home/user/freeswitch
git pull
./bootstrap.sh -j
./configure --prefix=/usr/local/freeswitch
make
sudo make install
sudo systemctl restart freeswitch
```

## 📚 Ek Kaynaklar

- **FreeSWITCH Docs:** https://freeswitch.org/confluence/
- **ESL Docs:** https://freeswitch.org/confluence/display/FREESWITCH/Event+Socket+Library
- **Brain-Core Integration:** `brain-core-integration-guide.md`
- **NetGSM Docs:** https://www.netgsm.com.tr/dokuman/

## ✅ Kurulum Sonrası Checklist

- [ ] FreeSWITCH başarıyla kuruldu
- [ ] `systemctl status freeswitch` aktif gösteriyor
- [ ] `fs_cli` ile bağlanabiliyorum
- [ ] Event Socket çalışıyor (port 8021)
- [ ] NetGSM SIP trunk ayarlandı
- [ ] Dialplan konfigüre edildi
- [ ] ESL Python binding çalışıyor
- [ ] Log dosyaları oluşuyor
- [ ] Güvenlik ayarları yapıldı

## 🆘 Yardım

Sorun yaşarsanız:

1. **Log dosyalarını kontrol edin:** `/usr/local/freeswitch/log/freeswitch.log`
2. **CLI debug mode:** `fs_cli> fsctl loglevel DEBUG`
3. **FreeSWITCH Community:** https://signalwire.community/
4. **GitHub Issues:** https://github.com/signalwire/freeswitch/issues

---

**Son Güncelleme:** 2025-11-18
**FreeSWITCH Version:** Latest (from master branch)
**Platform:** Debian 13 (Trixie)
