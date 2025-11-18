# FreeSWITCH + Brain-Core AI Voice Orchestrator

## 🎯 Proje Özeti

Bu proje, FreeSWITCH ile Brain-Core AI Orchestrator entegrasyonunu sağlayan, modüler bir AI-powered voice assistant sistemidir.

## 📁 Dosya Yapısı

```
freeswitch/
├── scripts/
│   ├── debian-install.sh              # Ana kurulum scripti
│   ├── mod_audio_stream_skeleton.c    # WebSocket audio streaming modülü (skeleton)
│   ├── brain-core-integration-guide.md # Detaylı entegrasyon kılavuzu
│   └── README.md                       # Bu dosya
```

## 🚀 Hızlı Başlangıç

### 1. FreeSWITCH Kurulumu

```bash
cd /home/user/freeswitch
sudo ./scripts/debian-install.sh
```

**Kurulum ne yapar?**
- ✅ Debian 13 için tüm bağımlılıkları yükler
- ✅ FreeSWITCH'i kaynak koddan derler (20-30 dk)
- ✅ Gerekli modülleri aktif eder:
  - `mod_event_socket` (Brain-Core ESL bağlantısı)
  - `mod_sofia` (SIP endpoint - NetGSM için)
  - `mod_verto` (WebSocket desteği)
  - Codec'ler (Opus, G729, AMR)
- ✅ Event Socket Library (ESL) yapılandırır
- ✅ Node.js 20.x yükler (Brain-Core için)
- ✅ Docker yükler (AI servisleri için)
- ✅ Systemd service oluşturur

### 2. Kurulum Sonrası Doğrulama

```bash
# FreeSWITCH başlat
sudo systemctl start freeswitch

# Durum kontrol
sudo systemctl status freeswitch

# Console'a bağlan
fs_cli

# Modülleri kontrol et
freeswitch@internal> show modules
```

### 3. ESL Test

```bash
# Telnet ile test
telnet localhost 8021
auth ClueCon
events plain ALL
```

## 🏗️ Mimari

```
┌─────────────────────────────────────────────────┐
│  TELEFON (NetGSM SIP Trunk)                     │
│       ↓ SIP/RTP                                  │
├─────────────────────────────────────────────────┤
│  FREESWITCH (Native)                            │
│   - mod_sofia (SIP)                             │
│   - mod_event_socket (ESL → Brain-Core)         │
│   - mod_audio_stream (WebSocket audio)          │
│       ↓ WebSocket + ESL                          │
├─────────────────────────────────────────────────┤
│  BRAIN-CORE ORCHESTRATOR (Docker - Node.js)     │
│   - WebSocket Server (Audio Stream)             │
│   - ESL Client (Call Control)                   │
│   - Pipeline Router (AI Service Selector)       │
│       ↓ HTTP/WebSocket                           │
├─────────────────────────────────────────────────┤
│  AI ADAPTER SERVICES (Docker)                   │
│   ┌──────┐  ┌──────┐  ┌──────┐                 │
│   │ STT  │→ │ LLM  │→ │ TTS  │                 │
│   └──────┘  └──────┘  └──────┘                 │
│   Deepgram  Gemini    ElevenLabs                │
└─────────────────────────────────────────────────┘
```

## 🔌 Brain-Core Entegrasyon Yöntemleri

### Yöntem 1: Event Socket Library (ESL)

**Amaç:** Call control, event monitoring, dialplan execution

**Brain-Core tarafında ESL Client kullanımı:**

```javascript
const ESL = require('modesl');
const conn = new ESL.Connection('localhost', 8021, 'ClueCon');

conn.on('esl::ready', () => {
  console.log('✅ FreeSWITCH ESL Connected');
  conn.subscribe(['CHANNEL_ANSWER', 'CHANNEL_HANGUP']);
});

conn.on('esl::event::CHANNEL_ANSWER::**', (event) => {
  const uuid = event.getHeader('Unique-ID');
  console.log(`📞 Call answered: ${uuid}`);
  // Start AI processing
});
```

### Yöntem 2: WebSocket Audio Streaming (mod_audio_stream)

**Amaç:** Real-time audio streaming to/from AI services

**Durum:** Custom modül - Geliştirilmesi gerekiyor

**Build adımları:**

```bash
cd /usr/src/freeswitch/src/mod/applications
mkdir mod_audio_stream
cp /home/user/freeswitch/scripts/mod_audio_stream_skeleton.c mod_audio_stream/

# Makefile oluştur ve derle (detaylar brain-core-integration-guide.md'de)
```

## 📋 Sıradaki Adımlar

1. ✅ **FreeSWITCH Kurulumu** - `sudo ./scripts/debian-install.sh`
2. ⚙️ **NetGSM SIP Trunk Konfigürasyonu** - `/etc/freeswitch/sip_profiles/external/netgsm.xml`
3. 🔧 **mod_audio_stream Geliştirme** - WebSocket audio streaming modülü
4. 🐳 **Brain-Core Deploy** - Node.js orchestrator (Docker container)
5. 🤖 **AI Services Deploy** - STT, LLM, TTS adapter containers (Docker)
6. 📞 **End-to-End Test** - NetGSM üzerinden test araması

## 📚 Dokümantasyon

- **[brain-core-integration-guide.md](./brain-core-integration-guide.md)** - Detaylı entegrasyon kılavuzu
  - ESL kullanımı örnekleri
  - WebSocket server implementasyonu
  - Dialplan konfigürasyonu
  - Test ve debugging

## 🔧 Kritik Konfigürasyon Dosyaları

| Dosya | Açıklama |
|-------|----------|
| `/etc/freeswitch/autoload_configs/event_socket.conf.xml` | ESL yapılandırması (127.0.0.1:8021) |
| `/etc/freeswitch/sip_profiles/external/netgsm.xml` | NetGSM SIP trunk ayarları |
| `/etc/freeswitch/dialplan/default/01_brain_core.xml` | Brain-Core routing dialplan |
| `/usr/local/freeswitch/conf/` | FreeSWITCH master config |

## 🎛️ Önemli Komutlar

```bash
# FreeSWITCH Control
sudo systemctl start freeswitch
sudo systemctl stop freeswitch
sudo systemctl status freeswitch
sudo systemctl restart freeswitch

# Console Commands
fs_cli                              # Console'a bağlan
fs_cli -x "show calls"              # Aktif aramalar
fs_cli -x "show channels"           # Aktif kanallar
fs_cli -x "sofia status"            # SIP durumu
fs_cli -x "reloadxml"               # Config reload
fs_cli -x "load mod_audio_stream"   # Modül yükle

# Logs
tail -f /usr/local/freeswitch/log/freeswitch.log
grep ERROR /usr/local/freeswitch/log/freeswitch.log
```

## 🧪 Test Scenarios

### Test 1: ESL Bağlantısı

```python
import ESL
conn = ESL.ESLconnection('localhost', '8021', 'ClueCon')
print("✅ Connected!" if conn.connected() else "❌ Failed")
```

### Test 2: WebSocket Server

```javascript
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 3000 });
wss.on('connection', (ws) => console.log('✅ FreeSWITCH connected'));
```

### Test 3: SIP Registration

```bash
fs_cli -x "sofia status gateway netgsm"
# Beklenen çıktı: State: REGED (Registered)
```

## ⚠️ Güvenlik & Performans

### Güvenlik
- 🔒 ESL şifresini değiştirin (varsayılan: "ClueCon")
- 🔒 SIP şifrelerini güvenli saklayın
- 🔒 Firewall kuralları ekleyin:
  ```bash
  ufw allow 5060/tcp    # SIP
  ufw allow 5060/udp    # SIP
  ufw allow 16384:32768/udp  # RTP
  ```

### Performans
- 🚀 Low-latency codec kullanın (Opus, G.729)
- 🚀 Audio chunk size: 20ms (default)
- 🚀 Sample rate: 16kHz (AI için optimal)
- 🚀 CPU scaling: `performance` mode

## 🆘 Troubleshooting

### Problem: FreeSWITCH başlamıyor

```bash
# Log kontrol
tail -100 /usr/local/freeswitch/log/freeswitch.log

# Permission kontrol
ls -la /usr/local/freeswitch/
# Owner: freeswitch:freeswitch olmalı
```

### Problem: ESL bağlanamıyor

```bash
# Port dinliyor mu?
netstat -tulpn | grep 8021

# Config doğru mu?
cat /etc/freeswitch/autoload_configs/event_socket.conf.xml
```

### Problem: NetGSM register olmuyor

```bash
fs_cli -x "sofia status"
fs_cli -x "sofia status gateway netgsm"

# Credentials kontrol
cat /etc/freeswitch/sip_profiles/external/netgsm.xml
```

## 📞 Destek

- FreeSWITCH Docs: https://freeswitch.org/confluence/
- ESL API: https://freeswitch.org/confluence/display/FREESWITCH/Event+Socket+Library
- Community: https://freeswitch.org/confluence/display/FREESWITCH/Community

---

**Başlamaya hazır mısınız?**

```bash
sudo ./scripts/debian-install.sh
```

Kurulum tamamlandıktan sonra detaylı entegrasyon için:
```bash
cat scripts/brain-core-integration-guide.md
```
