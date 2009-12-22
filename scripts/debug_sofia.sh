#!/bin/sh

export SOFIA_DEBUG=9
export NUA_DEBUG=9
export NTA_DEBUG=9
export NEA_DEBUG=9
export TPORT_DEBUG=9
export TPORT_LOG=1
export TPORT_DUMP=tport_sip.log
export SOA_DEBUG=9
export IPTSEC_DEBUG=9
export SU_DEBUG=9
./freeswitch
