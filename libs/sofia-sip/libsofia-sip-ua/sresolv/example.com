;
; Zone file for example.com
;
$TTL     60
@ 	IN SOA ns root (
	    2002042901 ; SERIAL
	    7200       ; REFRESH
	    600        ; RETRY
	    36000000   ; EXPIRE
	    60         ; MINIMUM
	    )
	NS	ns.example.com.
	;;  order pref flags service           regexp  replacement
        NAPTR 20 50 "s" "SIPS+D2T" "" _sips._tcp
        NAPTR 80 25 "s" "SIP+D2T" "" _sip._tcp
        NAPTR 40 15 "s" "SIP+D2U" "" _sip._udp
        NAPTR 50 15 "u" "TEST+D2U" "/(tst:([^@]+@))?example.com$/\\1operator.com/i" .
	MX 10 mgw00
	MX 10 mgw01
	MX 10 mgw02
	MX 100 smtp.example.net.
   	TXT foobar

ns	A	127.0.0.2
	AAAA	::2
	A6	0 ::2

oldns	A	194.2.188.133
	A6	0 3ffe:1200:3012:c000:210:a4ff:fe8d:6a46

_sip._udp	SRV 1 100 5060 sip00
		SRV 1 100 5060 sip01
		SRV 1 50 5060 sip02
_sips._tcp	SRV 3 100 5061 sip00
		SRV 5 10 5060 sip01
		SRV 4 50 5050 sip02
_sip._tcp	SRV 2 100 5060 sip00
		SRV 2 100 5060 sip01
		SRV 2 50 5060 sip02

_sips._udp	SRV 3 100 5061 sip00
		SRV 4 50 5051 sip02
		SRV 5 10 5061 sip01

cloop	CNAME	cloop0
cloop0	CNAME	cloop1
cloop1	CNAME	cloop2
cloop2	CNAME	cloop0

sip	CNAME	sip00

subnet  A6	0 3ff0:0::
labnet	A6	23 0:12:3012:: subnet
sublab	A6	48 0:0:0:c006:: labnet
mynet	A6	56 0:0:0:c006:: sublab
a6	A6	64 ::0a08:20ff:fe7d:e7ac mynet
alias6  A6      128 a6
full	A6	0 3ff0:12:3012:c006:0a08:20ff:fe7d:e7ac

sip00	A	194.2.188.133
	AAAA	3ff0:0010:3012:c000:02c0:95ff:fee2:4b78

sip01	A	194.2.188.134
	AAAA	3ff0:0012:3012:c006:0a08:20ff:fe7d:e7ac

sip02	A	194.2.188.135
	AAAA	3ffe:1200:3012:c006:0206:5bff:fe55:462f

sip03	A	194.2.188.136
	AAAA	3ffe:1200:3012:c000:0206:5bff:fe55:4630

mgw00	A	194.2.188.130
	AAAA	3ffe:1200:3012:c000:02c0:95ff:fee2:4b78

mgw01	A	194.2.188.131
	AAAA	3ffe:1200:3012:c000:0a08:20ff:fe7d:e7ac

mgw02	A	194.2.188.132
	AAAA	3ffe:1200:3012:c000:0206:5bff:fe55:462f

a	A	194.2.188.137
aaaa	AAAA	3ffe:1200:3012:c000:0206:5bff:fe55:462f

$ORIGIN iptel
@	A	194.2.188.133
	AAAA	3ffe:1200:3012:c000:200:5eff:fe00:106
	NAPTR 50 50 "s" "SIPS+D2T" "" _sips._tcp.iptel
        NAPTR 90 50 "s" "SIP+D2T"  "" _sip._tcp.iptel
        NAPTR 80 50 "s" "SIP+D2U"  "" _sip._udp.iptel

_sip._udp  SRV 0 100 5060 iptel
_sip._tcp  SRV 1 100 5060 iptel
_sips._tcp SRV 1 100 5061 iptel
