import sys, time
def onDTMF(input, itype, buf, buflen):
  print "input=",input
  print "itype=",itype
  print "buf=",buf
  print "buflen",buflen
  if input == "#":
      return 1
  else:
      return 0
console_log("1","test from my python program\n")
session.answer()
session.set_dtmf_callback(onDTMF)
session.set_tts_parms("cepstral", "david")
session.play_file("/root/test.gsm", "")
session.speak_text("Please enter telephone number with area code and press pound sign. ")
input = session.get_digits("", 11, "*#", 10000)
console_log("1","result from get digits is "+ input +"\n")
phone_number = session.play_and_get_digits(5, 11, 3, 10000, "*#",
                                           "/sounds/test.gsm",
                                           "/sounds/invalid.gsm",
                                           "",
                                           "^17771112222$");
console_log("1","result from play_and_get_digits is "+ phone_number +"\n")
session.transfer("1000", "XML", "default")
session.hangup("1")
