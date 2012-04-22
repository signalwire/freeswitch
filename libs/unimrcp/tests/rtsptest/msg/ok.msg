RTSP/1.0 200 OK
CSeq:2
Transport:RTP/AVP;unicast;client_port=46456-46457;server_port=46460-46461
Session:12345678
Content-Length:188
Content-Type:application/sdp

v=0
o=- 3211724219 3211724219 IN IP4 10.3.2.88
s=Media Server
c=IN IP4 0.0.0.0
t=0 0
m=audio 46460 RTP/AVP 0 96
a=rtpmap:0 pcmu/8000
a=rtpmap:96 telephone-event/8000
a=fmtp:96 0-15