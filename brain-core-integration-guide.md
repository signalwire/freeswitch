# FreeSWITCH + Brain-Core Integration Guide

Bu dokümantasyon FreeSWITCH'in Brain-Core orchestrator ile nasıl entegre edileceğini açıklar.

## Mimari Özet

```
NetGSM SIP Trunk
     ↓ (SIP/RTP)
FreeSWITCH (mod_sofia)
     ↓ (Event Socket Library - ESL)
Brain-Core Orchestrator (Node.js)
     ↓ (WebSocket/HTTP)
AI Adapter Services (STT/LLM/TTS)
```

## 1. Kurulum

### FreeSWITCH Kurulumu

```bash
# Script'i executable yap
chmod +x install-debian13.sh

# Root olarak çalıştır
sudo ./install-debian13.sh
```

Kurulum yaklaşık 30-60 dakika sürecektir (sunucu performansına bağlı).

## 2. Event Socket Library (ESL) Konfigürasyonu

### Event Socket'i Etkinleştir

Event Socket, Brain-Core'un FreeSWITCH ile iletişim kurması için kullanılır.

**Config Dosyası:** `/usr/local/freeswitch/conf/autoload_configs/event_socket.conf.xml`

```xml
<configuration name="event_socket.conf" description="Socket Client">
  <settings>
    <!-- Brain-Core'un bağlanacağı adres ve port -->
    <param name="nat-map" value="false"/>
    <param name="listen-ip" value="127.0.0.1"/>
    <param name="listen-port" value="8021"/>

    <!-- Güvenlik için güçlü bir şifre belirleyin -->
    <param name="password" value="YourSecurePassword123!"/>

    <!-- Diğer ayarlar -->
    <param name="apply-inbound-acl" value="lan"/>
  </settings>
</configuration>
```

**ÖNEMLI:** Production'da `password` değerini mutlaka değiştirin!

### ESL Modülünü Yükle

**Config Dosyası:** `/usr/local/freeswitch/conf/autoload_configs/modules.conf.xml`

```xml
<configuration name="modules.conf" description="Modules">
  <modules>
    <!-- ... diğer modüller ... -->

    <!-- Event Socket Library -->
    <load module="mod_event_socket"/>

    <!-- ... diğer modüller ... -->
  </modules>
</configuration>
```

## 3. SIP Trunk Konfigürasyonu (NetGSM)

### External SIP Profile Ayarları

**Config Dosyası:** `/usr/local/freeswitch/conf/sip_profiles/external/netgsm.xml`

```xml
<include>
  <gateway name="netgsm">
    <param name="realm" value="sip.netgsm.com.tr"/>
    <param name="username" value="YOUR_NETGSM_USERNAME"/>
    <param name="password" value="YOUR_NETGSM_PASSWORD"/>

    <!-- NetGSM SIP sunucu adresi -->
    <param name="proxy" value="sip.netgsm.com.tr"/>
    <param name="register" value="true"/>
    <param name="register-transport" value="udp"/>

    <!-- Codec ayarları -->
    <param name="codec-prefs" value="PCMU,PCMA,G729"/>

    <!-- NAT ayarları -->
    <param name="extension-in-contact" value="true"/>

    <!-- Caller ID -->
    <param name="caller-id-in-from" value="true"/>
  </gateway>
</include>
```

**NetGSM bilgilerinizi buraya girin:**
- `YOUR_NETGSM_USERNAME`: NetGSM kullanıcı adınız
- `YOUR_NETGSM_PASSWORD`: NetGSM şifreniz

## 4. Dialplan Konfigürasyonu (AI Call Routing)

### Brain-Core'a Gelen Aramaları Yönlendir

**Config Dosyası:** `/usr/local/freeswitch/conf/dialplan/default/01_brain_core.xml`

```xml
<include>
  <extension name="brain-core-inbound">
    <condition field="destination_number" expression="^(YOUR_DID_NUMBER)$">

      <!-- Arayanı karşıla -->
      <action application="answer"/>

      <!-- Sessizliği önle -->
      <action application="set" data="ring_ready=true"/>

      <!-- Call UUID'yi değişkende sakla -->
      <action application="set" data="call_uuid=${uuid}"/>

      <!-- Arama bilgilerini Brain-Core'a bildir (HTTP/WebSocket) -->
      <action application="curl" data="http://localhost:3000/api/calls/inbound json {&quot;uuid&quot;:&quot;${uuid}&quot;,&quot;caller&quot;:&quot;${caller_id_number}&quot;,&quot;called&quot;:&quot;${destination_number}&quot;}"/>

      <!-- Brain-Core kontrolünde bekle (ESL üzerinden kontrol edilecek) -->
      <action application="park"/>

    </condition>
  </extension>
</include>
```

**DID numaranızı girin:**
- `YOUR_DID_NUMBER`: NetGSM'den aldığınız telefon numarası

## 5. Audio Stream Konfigürasyonu

Brain-Core'un gerçek zamanlı ses akışını alabilmesi için:

### RTP Codec Ayarları

**Config Dosyası:** `/usr/local/freeswitch/conf/autoload_configs/switch.conf.xml`

```xml
<configuration name="switch.conf" description="Core Configuration">
  <settings>
    <!-- RTP port range (Brain-Core için) -->
    <param name="rtp-start-port" value="16384"/>
    <param name="rtp-end-port" value="32768"/>

    <!-- Audio sampling rate (AI için optimize) -->
    <param name="default-sample-rate" value="16000"/>

    <!-- Audio format (AI için optimize) -->
    <param name="default-sample-format" value="s16"/>
  </settings>
</configuration>
```

### Media Handling

FreeSWITCH'ten audio stream almak için **Event Socket** kullanılacak:

```javascript
// Brain-Core tarafında (Node.js örneği)
const ESL = require('modesl');

const conn = new ESL.Connection('127.0.0.1', 8021, 'YourSecurePassword123!', function() {
  conn.subscribe(['CHANNEL_ANSWER', 'CHANNEL_HANGUP', 'DTMF']);

  conn.on('esl::event::CHANNEL_ANSWER', (event) => {
    const uuid = event.getHeader('Unique-ID');

    // UUID ile audio stream al
    conn.api(`uuid_record ${uuid} /tmp/recording_${uuid}.wav`);

    // Veya real-time audio için:
    // conn.sendRecv(`sendmsg ${uuid}\ncall-command: execute\nexecute-app-name: record_session\nexecute-app-arg: /tmp/stream_${uuid}.raw`);
  });
});
```

## 6. Brain-Core ESL Client Örneği

### Python ESL Client (Test Amaçlı)

```python
#!/usr/bin/env python3
import ESL

# FreeSWITCH'e bağlan
con = ESL.ESLconnection("127.0.0.1", "8021", "YourSecurePassword123!")

if con.connected():
    print("FreeSWITCH'e bağlandı!")

    # Event'leri dinle
    con.events("plain", "CHANNEL_ANSWER CHANNEL_HANGUP")

    while True:
        e = con.recvEvent()

        if e:
            event_name = e.getHeader("Event-Name")
            uuid = e.getHeader("Unique-ID")

            print(f"Event: {event_name}, UUID: {uuid}")

            if event_name == "CHANNEL_ANSWER":
                # Arama cevaplandı - Brain-Core pipeline'ını başlat
                print(f"Call {uuid} answered - Starting AI pipeline...")

            elif event_name == "CHANNEL_HANGUP":
                # Arama kapandı
                print(f"Call {uuid} hung up")
else:
    print("FreeSWITCH'e bağlanılamadı!")
```

### Node.js ESL Client Örneği

```javascript
// npm install modesl
const ESL = require('modesl');

const conn = new ESL.Connection('127.0.0.1', 8021, 'YourSecurePassword123!', () => {
  console.log('FreeSWITCH\'e bağlandı!');

  // Event subscription
  conn.subscribe(['CHANNEL_ANSWER', 'CHANNEL_HANGUP', 'DTMF']);

  conn.on('esl::event::CHANNEL_ANSWER', (event) => {
    const uuid = event.getHeader('Unique-ID');
    const caller = event.getHeader('Caller-Caller-ID-Number');

    console.log(`Call answered - UUID: ${uuid}, Caller: ${caller}`);

    // Brain-Core pipeline başlat
    startAIPipeline(uuid, caller);
  });

  conn.on('esl::event::CHANNEL_HANGUP', (event) => {
    const uuid = event.getHeader('Unique-ID');
    console.log(`Call hung up - UUID: ${uuid}`);

    // Pipeline'ı durdur
    stopAIPipeline(uuid);
  });
});

function startAIPipeline(uuid, caller) {
  // STT -> LLM -> TTS pipeline'ını başlat
  console.log(`Starting AI pipeline for ${uuid}`);

  // Audio stream'i al ve Brain-Core'a gönder
  conn.api(`uuid_record ${uuid} /tmp/recording_${uuid}.wav`, (response) => {
    console.log('Recording started:', response.getBody());
  });
}

function stopAIPipeline(uuid) {
  console.log(`Stopping AI pipeline for ${uuid}`);
}
```

## 7. FreeSWITCH Başlatma

```bash
# FreeSWITCH'i başlat
sudo systemctl start freeswitch

# Durum kontrolü
sudo systemctl status freeswitch

# Logları izle
sudo tail -f /usr/local/freeswitch/log/freeswitch.log

# CLI'ya bağlan
/usr/local/freeswitch/bin/fs_cli

# CLI'dan ESL durumunu kontrol et
fs_cli> event_socket status
```

## 8. Güvenlik ve Production Ayarları

### Firewall Kuralları

```bash
# ESL port'u sadece localhost'tan erişilebilir
sudo ufw allow from 127.0.0.1 to any port 8021 proto tcp

# SIP (NetGSM için)
sudo ufw allow 5060/udp
sudo ufw allow 5080/udp

# RTP
sudo ufw allow 16384:32768/udp
```

### ESL Güvenlik

1. **Güçlü şifre kullanın** - `event_socket.conf.xml` içinde
2. **ACL (Access Control List) kullanın** - Sadece Brain-Core IP'sinden bağlantı kabul edin
3. **TLS/SSL kullanın** - Production'da şifreli bağlantı kullanın

### Log Levels

Development sırasında detaylı log:

```bash
fs_cli> fsctl loglevel DEBUG
```

Production'da performans için:

```bash
fs_cli> fsctl loglevel WARNING
```

## 9. Troubleshooting

### ESL Bağlantı Problemi

```bash
# ESL port'u dinliyor mu?
sudo netstat -tlnp | grep 8021

# FreeSWITCH loglarını kontrol et
tail -f /usr/local/freeswitch/log/freeswitch.log | grep -i "event_socket"
```

### SIP Registration Problemi

```bash
# CLI'dan gateway durumunu kontrol et
fs_cli> sofia status gateway netgsm

# SIP profile'ı restart et
fs_cli> sofia profile external restart
```

### Audio Problemi

```bash
# Codec listesini kontrol et
fs_cli> show codec

# RTP paketlerini kontrol et
sudo tcpdump -i any -n port 16384:32768
```

## 10. Sonraki Adımlar

1. ✅ FreeSWITCH kurulumu tamamlandı
2. ⏭️ Brain-Core Orchestrator kurulumu
3. ⏭️ AI Adapter Services (STT/LLM/TTS) kurulumu
4. ⏭️ Entegrasyon testleri
5. ⏭️ Production deployment

## Kaynaklar

- FreeSWITCH Docs: https://freeswitch.org/confluence/
- ESL Documentation: https://freeswitch.org/confluence/display/FREESWITCH/Event+Socket+Library
- mod_event_socket: https://freeswitch.org/confluence/display/FREESWITCH/mod_event_socket
- NetGSM Docs: https://www.netgsm.com.tr/dokuman/
