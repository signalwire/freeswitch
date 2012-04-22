RTSP/1.0 200 OK
CSeq:3
Transport:RTP/AVP;unicast;client_port=46456-46457;
         server_port=46460-46461;mode=record;ttl=127
Session:12345678

SETUP rtsp://media.server.com/media/synthesizer RTSP/1.0
CSeq:2
Transport:RTP/AVP;unicast;client_port=46456-46457
Content-Type:application/sdp
Content-Length:190

v=0
o=- 123 456 IN IP4 10.0.0.1
s=Media Server
p=+1-888-555-1212
c=IN IP4 0.0.0.0
t=0 0
m=audio 0 RTP/AVP 0 96
a=rtpmap:0 pcmu/8000
a=rtpmap:96 telephone-event/8000
a=fmtp:96 0-15
ANNOUNCE rtsp://media.server.com/media/synthesizer RTSP/1.0
CSeq:4
Session:12345678
Content-Type:application/mrcp
Content-Length:567

SPEAK 543257 MRCP/1.0
Kill-On-Barge-In:false
Voice-gender:neutral
Prosody-volume:medium
Content-Type:application/synthesis+ssml
Content-Length:412

<?xml version="1.0"?>
<speak>
<paragraph>
       <sentence>You have 4 new messages.</sentence>
       <sentence>The first is from <say-as type="name">Stephanie Williams</say-as> 
       <mark name="Stephanie"/> and arrived at <break/>
       <say-as type="time">3:45pm</say-as>.</sentence>
       
       <sentence>The subject is <prosody rate="-20%">ski trip</prosody></sentence>
</paragraph>
</speak>