from freeswitch import *
from py_modules.speechtools import Grammar, SpeechDetect
from py_modules.speechtools import SpeechObtainer

import time, os

VOICE_ENGINE = "cepstral"
VOICE = "William"
GRAMMAR_ROOT = "/usr/src/freeswitch_trunk/scripts"

"""
Example speech recognition application in python.  

How to make this work:

* Get mod_openmrcp working along with an MRCP asr server
* Add /usr/src/freeswitch/scripts or equivalent to your PYTHONPATH
* Restart freeswitch
* Create $GRAMMAR_ROOT/mainmenu.xml from contents in mainmenu() comments

"""

class RecipeWizard:

    def __init__(self, session):
        self.session=session
        self.session.set_tts_parms(VOICE_ENGINE, VOICE)        
        self.main()

    def main(self):

        console_log("debug", "recipe wizard main()\n")        
        self.speechdetect = SpeechDetect(self.session, "openmrcp", "127.0.0.1");
        self.speechobtainer = SpeechObtainer(speech_detect=self.speechdetect,
                                             required_phrases=1,
                                             wait_time=5000,
                                             max_tries=3)
        gfile = os.path.join(GRAMMAR_ROOT, "mainmenu.xml")
        self.grammar = Grammar("mainmenu", gfile,"input",80,90)
        self.speechobtainer.setGrammar(self.grammar);
        console_log("debug", "calling speechobtainer.run()\n")
        self.speechobtainer.detectSpeech()
        self.session.speak("Hello. Welcome to the recipe wizard. Drinks or food?")
        result = self.speechobtainer.run()
        console_log("debug", "speechobtainer.run() result: %s\n" % result)
        if result:
            self.session.speak("Received result.  Result is: %s" % result[0])
        else:
            self.session.speak("Sorry, I did not hear you")
            
        console_log("debug", "speechobtainer.run() finished\n")        

def mainmenu():
    """
    <!DOCTYPE grammar PUBLIC "-//W3C//DTD GRAMMAR 1.0//EN"
             "http://www.w3.org/TR/speech-grammar/grammar.dtd">

    <grammar xmlns="http://www.w3.org/2001/06/grammar" xml:lang="en"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.w3.org/2001/06/grammar
                      http://www.w3.org/TR/speech-grammar/grammar.xsd"
      version="1.0" mode="voice" root="root">


    <rule id="root" scope="public">

        <rule id="main">
          <one-of>
         <item weight="10">drinks</item>
         <item weight="2">food</item>
          </one-of>
        </rule>

    </rule>

    </grammar>

    """
    pass

def handler(uuid):
    session = PySession(uuid)
    session.answer()
    rw = RecipeWizard(session)
    session.hangup("1")


