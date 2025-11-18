# FreeSWITCH + Brain-Core Orchestrator Integration Guide

## 🎯 Overview

Bu dokümantasyon, FreeSWITCH'in Brain-Core AI Orchestrator ile entegrasyonu için detaylı kılavuzdur.

## 📋 Architecture Recap

```
┌─────────────────────────────────────────────────┐
│  1. TELEFON (NetGSM SIP Trunk)                  │
│     ↓ SIP/RTP                                    │
├─────────────────────────────────────────────────┤
│  2. FREESWITCH (Native - /usr/local/freeswitch) │
│     - mod_sofia (SIP Stack)                     │
│     - mod_event_socket (ESL for Brain-Core)     │
│     - mod_audio_stream (WebSocket audio)        │
│     ↓ WebSocket Binary Audio + ESL              │
├─────────────────────────────────────────────────┤
│  3. BRAIN-CORE ORCHESTRATOR (Docker - Node.js)  │
│     - WebSocket Server (Audio Streaming)        │
│     - ESL Client (Call Control)                 │
│     - Pipeline Router (AI Service Selector)     │
│     ↓ HTTP/WebSocket to AI Services             │
├─────────────────────────────────────────────────┤
│  4. AI ADAPTER SERVICES (Docker Containers)     │
│     STT → LLM → TTS (Modular Pipeline)          │
│     or STS (Speech-to-Speech Direct)            │
└─────────────────────────────────────────────────┘
```

## 🔧 Installation

### Step 1: Install FreeSWITCH

```bash
cd /home/user/freeswitch
chmod +x scripts/debian-install.sh
sudo ./scripts/debian-install.sh
```

Bu script:
- ✅ Tüm sistem bağımlılıklarını yükler
- ✅ FreeSWITCH'i kaynak koddan derler
- ✅ Gerekli modülleri aktif eder
- ✅ Event Socket Library (ESL) yapılandırır
- ✅ Node.js ve Docker yükler
- ✅ Systemd service oluşturur

**Kurulum süresi:** ~20-30 dakika (AWS t3.medium instance için)

### Step 2: Verify Installation

```bash
# FreeSWITCH durumunu kontrol et
sudo systemctl status freeswitch

# FreeSWITCH başlat
sudo systemctl start freeswitch

# FreeSWITCH console'a bağlan
fs_cli

# Console'da modülleri kontrol et
freeswitch@internal> show modules
```

### Step 3: Test Event Socket Library (ESL)

```bash
# Telnet ile ESL'e bağlan
telnet localhost 8021

# Şifre gir (varsayılan: ClueCon)
auth ClueCon

# Event'leri dinle
events plain ALL
```

## 🔌 Brain-Core Integration

### Integration Method 1: Event Socket Library (ESL)

**Kullanım:** Call control, event monitoring, dialplan execution

**Brain-Core'da ESL Client Örneği (Node.js):**

```javascript
// brain-core/lib/freeswitch-esl-client.js
const ESL = require('modesl');

class FreeSwitchESLClient {
  constructor(host = 'localhost', port = 8021, password = 'ClueCon') {
    this.connection = new ESL.Connection(host, port, password);

    this.connection.on('esl::ready', () => {
      console.log('✅ Connected to FreeSWITCH ESL');
      this.subscribeToEvents();
    });

    this.connection.on('esl::event::**', (event) => {
      this.handleEvent(event);
    });
  }

  subscribeToEvents() {
    // Subscribe to call events
    this.connection.subscribe([
      'CHANNEL_CREATE',
      'CHANNEL_ANSWER',
      'CHANNEL_HANGUP',
      'CUSTOM'
    ]);
  }

  handleEvent(event) {
    const eventName = event.getHeader('Event-Name');
    const callUUID = event.getHeader('Unique-ID');

    switch (eventName) {
      case 'CHANNEL_ANSWER':
        console.log(`📞 Call answered: ${callUUID}`);
        this.onCallAnswered(callUUID, event);
        break;

      case 'CHANNEL_HANGUP':
        console.log(`📵 Call hangup: ${callUUID}`);
        this.onCallHangup(callUUID, event);
        break;
    }
  }

  async onCallAnswered(callUUID, event) {
    // Start audio streaming to Brain-Core
    await this.executeApp(callUUID, 'stream_to_brain');
  }

  async executeApp(uuid, app, args = '') {
    return new Promise((resolve, reject) => {
      this.connection.api(`uuid_${app} ${uuid} ${args}`, (res) => {
        resolve(res.getBody());
      });
    });
  }

  async playAudio(uuid, audioFile) {
    await this.executeApp(uuid, 'playback', audioFile);
  }

  async hangup(uuid) {
    await this.executeApp(uuid, 'kill', 'NORMAL_CLEARING');
  }
}

module.exports = FreeSwitchESLClient;
```

### Integration Method 2: WebSocket Audio Streaming (mod_audio_stream)

**Kullanım:** Real-time audio streaming to/from AI services

**Status:** Custom module - Needs to be developed

**Build mod_audio_stream:**

```bash
cd /home/user/freeswitch/src/mod/applications
mkdir mod_audio_stream
cp /home/user/freeswitch/scripts/mod_audio_stream_skeleton.c mod_audio_stream/mod_audio_stream.c

# Create Makefile.am
cat > mod_audio_stream/Makefile.am <<'EOF'
include $(top_srcdir)/build/modmake.rulesam
MODNAME = mod_audio_stream

mod_LTLIBRARIES = mod_audio_stream.la
mod_audio_stream_la_SOURCES = mod_audio_stream.c
mod_audio_stream_la_CFLAGS = $(AM_CFLAGS) -I/usr/include/libwebsockets
mod_audio_stream_la_LIBADD = $(switch_builddir)/libfreeswitch.la -lwebsockets
mod_audio_stream_la_LDFLAGS = -avoid-version -module -no-undefined -shared
EOF

# Rebuild FreeSWITCH with mod_audio_stream
cd /usr/src/freeswitch
echo "applications/mod_audio_stream" >> modules.conf
make mod_audio_stream
make mod_audio_stream-install

# Load module
fs_cli -x "load mod_audio_stream"
```

**Brain-Core WebSocket Server Örneği:**

```javascript
// brain-core/lib/audio-websocket-server.js
const WebSocket = require('ws');

class AudioWebSocketServer {
  constructor(port = 3000) {
    this.wss = new WebSocket.Server({ port });
    this.activeCalls = new Map();

    this.wss.on('connection', (ws, req) => {
      console.log('🎧 Audio stream connected from FreeSWITCH');

      ws.on('message', async (audioData) => {
        // audioData is binary audio chunk (L16 PCM)
        await this.processAudioChunk(ws, audioData);
      });

      ws.on('close', () => {
        console.log('🔌 Audio stream disconnected');
      });
    });
  }

  async processAudioChunk(ws, audioData) {
    // 1. Send to STT service
    const text = await this.sttAdapter.transcribe(audioData);

    if (text) {
      // 2. Send to LLM service
      const response = await this.llmAdapter.chat(text);

      // 3. Send to TTS service
      const audioResponse = await this.ttsAdapter.synthesize(response);

      // 4. Send back to FreeSWITCH
      ws.send(audioResponse);
    }
  }
}

module.exports = AudioWebSocketServer;
```

## 📝 Configuration Files

### 1. FreeSWITCH Event Socket (/etc/freeswitch/autoload_configs/event_socket.conf.xml)

```xml
<configuration name="event_socket.conf" description="Event Socket Configuration">
  <settings>
    <param name="nat-map" value="false"/>
    <param name="listen-ip" value="127.0.0.1"/>
    <param name="listen-port" value="8021"/>
    <param name="password" value="ClueCon"/>
  </settings>
</configuration>
```

### 2. SIP Profile for NetGSM (/etc/freeswitch/sip_profiles/external/netgsm.xml)

```xml
<include>
  <gateway name="netgsm">
    <param name="username" value="YOUR_NETGSM_USERNAME"/>
    <param name="password" value="YOUR_NETGSM_PASSWORD"/>
    <param name="realm" value="netgsm.com.tr"/>
    <param name="proxy" value="netgsm.com.tr"/>
    <param name="register" value="true"/>
    <param name="caller-id-in-from" value="true"/>
    <param name="ping" value="30"/>
  </gateway>
</include>
```

### 3. Dialplan for Brain-Core Routing (/etc/freeswitch/dialplan/default/01_brain_core.xml)

```xml
<include>
  <extension name="brain_core_ai_handler">
    <condition field="destination_number" expression="^(YOUR_DID_NUMBER)$">
      <!-- Answer the call -->
      <action application="answer"/>

      <!-- Set variables -->
      <action application="set" data="call_timeout=30"/>
      <action application="set" data="hangup_after_bridge=true"/>

      <!-- Start audio streaming to Brain-Core -->
      <action application="stream_to_brain"/>

      <!-- Fallback if stream fails -->
      <action application="playback" data="/usr/local/freeswitch/sounds/en/us/callie/ivr/8000/ivr-please_try_again.wav"/>
      <action application="hangup"/>
    </condition>
  </extension>
</include>
```

## 🧪 Testing

### Test 1: ESL Connection

```bash
# Python test script
cat > /tmp/test_esl.py <<'EOF'
import ESL

conn = ESL.ESLconnection('localhost', '8021', 'ClueCon')
if conn.connected():
    print("✅ ESL Connected!")
    conn.events('plain', 'all')
    print("✅ Subscribed to events")
else:
    print("❌ ESL Connection failed")
EOF

python3 /tmp/test_esl.py
```

### Test 2: WebSocket Audio Server

```bash
# Node.js test server
cat > /tmp/test_ws_server.js <<'EOF'
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 3000 });

wss.on('connection', (ws) => {
  console.log('✅ WebSocket connected');

  ws.on('message', (data) => {
    console.log(`📦 Received ${data.length} bytes`);
    // Echo back
    ws.send(data);
  });
});

console.log('🎧 WebSocket server listening on ws://localhost:3000');
EOF

node /tmp/test_ws_server.js
```

## 📊 Monitoring & Debugging

### FreeSWITCH Logs

```bash
# Real-time log tail
tail -f /usr/local/freeswitch/log/freeswitch.log

# Check for errors
grep ERROR /usr/local/freeswitch/log/freeswitch.log

# Console commands
fs_cli -x "show calls"
fs_cli -x "show channels"
fs_cli -x "sofia status"
```

### Performance Metrics

```bash
# CPU & Memory usage
htop

# Network connections
netstat -tulpn | grep freeswitch

# Audio latency check
fs_cli -x "show codec"
```

## 🚀 Next Steps

1. ✅ **Install FreeSWITCH** - `sudo ./scripts/debian-install.sh`
2. ⚙️ **Configure NetGSM SIP Trunk** - Edit `/etc/freeswitch/sip_profiles/external/netgsm.xml`
3. 🔧 **Build mod_audio_stream** - Compile custom WebSocket audio module
4. 🐳 **Deploy Brain-Core** - Docker container with Node.js orchestrator
5. 🤖 **Deploy AI Services** - STT, LLM, TTS adapter containers
6. 📞 **Test End-to-End** - Make a test call through NetGSM

## 📚 Additional Resources

- [FreeSWITCH Documentation](https://freeswitch.org/confluence/)
- [Event Socket Library (ESL) API](https://freeswitch.org/confluence/display/FREESWITCH/Event+Socket+Library)
- [FreeSWITCH Dialplan XML](https://freeswitch.org/confluence/display/FREESWITCH/XML+Dialplan)
- [Sofia-SIP Module](https://freeswitch.org/confluence/display/FREESWITCH/mod_sofia)

## ⚠️ Important Notes

- **Security:** Change default ESL password from "ClueCon"
- **Firewall:** Open ports 5060 (SIP), 16384-32768 (RTP)
- **Performance:** Use low-latency codecs (Opus, G.729) for AI integration
- **Monitoring:** Set up alerts for call quality degradation

---

**Ready to start?** Run the installation script:

```bash
sudo ./scripts/debian-install.sh
```
