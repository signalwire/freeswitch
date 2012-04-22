SETUP rtsp://media.server.com/media/synthesizer RTSP/1.0
CSeq:2
Transport:RTP/AVP;unicast;client_port=46456-46457
Content-Type:application/sdp
Content-Length:188

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