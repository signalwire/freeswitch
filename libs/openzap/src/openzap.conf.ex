[openzap]
load => wanpipe
;load => zt

[+wanpipe.conf]
[span]
enabled => 1
fxs-channel => 1:3-4


[span]
enabled => 1
fxo-channel => 1:1-2


[+zt.conf]

[span]
enabled => 0
b-channel => 1-23
d-channel=> 24

[span]
enabled => 0
b-channel => 25-47
d-channel=> 48

[span]
enabled => 0
fxo-channel => 49
fxs-channel => 50

[+tones.conf]
[us]
dial => %(1000,0,350,440)
ring => %(2000,4000,440,480)
busy => %(500,500,480,620)
attn => %(100,100,1400,2060,2450,2600)
