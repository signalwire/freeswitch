# Please see latest version of this script at
# http://wiki.freeswitch.org/wiki/Mod_python
# before reporting errors

from freeswitch import *


def onDTMF(input, itype, funcargs):
    console_log("1", "\n\nonDTMF input: %s\n" % input)
    if input == "5":
        return "pause"
    if input == "3":
        return "seek:+60000"
    if input == "1":
        return "seek:-60000"
    if input == "0":
        return "stop"
    return None  # will make the streamfile audio stop


def handler(uuid):
    console_log("1", "... test from my python program\n")
    session = PySession(uuid)
    session.answer()
    session.setDTMFCallback(onDTMF, "")
    session.set_tts_parms("cepstral", "david")
    session.playFile("/path/to/your.mp3", "")
    session.speak("Please enter telephone number with area code and press pound sign. ")
    input = session.getDigits("", 11, "*#", "#", 10000)
    console_log("1", "result from get digits is %s\n" % input)
    phone_number = session.playAndGetDigits(5, 11, 3, 10000, "*#",
                                            "/sounds/test.gsm",
                                            "/sounds/invalid.gsm",
                                            "",
                                            "^17771112222$")
    console_log("1", "result from play_and_get_digits is %s\n" % phone_number)
    session.transfer("1000", "XML", "default")
    session.hangup("1")
