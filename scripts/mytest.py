import sys 
from _freeswitch import *

print "Hello World"
print sys.path
print dir()
print sys.argv

uuid = sys.argv[0]
fs_consol_log("1","test from my python program\n")
fs_consol_clean("This is fs_consol_clean\n")
fs_consol_clean("My uuid is " + uuid + "\n")

session = fs_core_session_locate(uuid)

fs_channel_answer(session)

fs_switch_ivr_session_transfer(session, "1234", "XML", "default")
