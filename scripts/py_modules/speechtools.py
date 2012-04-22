from freeswitch import *
from xml.dom import minidom

VOICE_ENGINE = "cepstral"
VOICE = "William"

"""
A few classes that make it easier to write speech applications
using Python.  It is roughly modelled after the equivalent that
is written in JavaScript.

Status: should work, but not yet complete.  some pending items
are mentioned in comments
"""

class Grammar:
    def __init__(self, name, path, obj_path,
                 min_score=1, confirm_score=400, halt=False):
        """
        @param name - name of grammar to reference it later
        @param path - path to xml grammar file
        @param obj_path - xml path to find interpretation from root
                          in result xml, eg, 'interpretation'
        @param min_score - score threshold to accept result
        @param confirm_score - if score below this threshold, ask user
                               if they are sure this is correct
        @param halt - not sure what was used for in js, currently unused
        """
        self.name=name
        self.path=path
        self.obj_path=obj_path
        self.min_score=min_score
        self.confirm_score=confirm_score
        self.halt=halt

    
class SpeechDetect:

    def __init__(self, session, module_name, ip_addr):
        self.session=session
        self.module_name=module_name
        self.ip_addr=ip_addr
        self.grammars = {}

    def addGrammar(self, grammar):
        self.grammars[grammar.name]=grammar

    def setGrammar(self, name):
        self.grammar = self.grammars[name]

    def detectSpeech(self):
        # TODO: we might not always want to call detect_speech
        # with this cmd, see js version for other options
        # also see detect_speech_function() in mod_dptools.c
        cmd = "%s %s %s %s" % (self.module_name,
                               self.grammar.name,
                               self.grammar.path,
                               self.ip_addr)
        console_log("debug", "calling detect_speech with: %s\n" % cmd)
        self.session.execute("detect_speech", cmd)
        console_log("debug", "finished calling detect_speech\n")
        
class SpeechObtainer:

    def __init__(self, speech_detect, required_phrases, wait_time, max_tries):
        """
        @param speech_detect - the speech detect object, which holds a
                               reference to underlying session and can
                               be re-used by many SpeechObtainers
        @param required_phrases - the number of required phrases from the
                                  grammar.  for example if its prompting for
                                  the toppings on a sandwhich and min toppings
                                  is 3, use 3.  normally will be 1.
        @param wait_time - the time, in millisconds, to wait for
                           input during each loop iteration
        @param max_tries - this number multiplied by wait time gives the
                           'total wait time' before we give up and return
                           partial or no result
        """
        self.speech_detect=speech_detect
        self.required_phrases=required_phrases
        self.wait_time=wait_time
        self.max_tries=max_tries        

        self.detected_phrases = []
        self.failed = False
        
    def setGrammar(self, grammar):
        """
        @param grammar - instance of grammar class
        """
        self.grammar=grammar
        self.speech_detect.addGrammar(grammar)
        self.speech_detect.setGrammar(self.grammar.name)

    def detectSpeech(self):
        self.speech_detect.detectSpeech()
        
    def run(self):
        """
        start speech detection with the current grammar,
        and listen for results from asr engine.  once a result
        has been returned, return it to caller
        """

        def dtmf_handler(input, itype, funcargs):
            console_log("INFO","\n\nDTMF itype: %s\n" % itype)
            if itype == 1: # TODO!! use names for comparison instead of number
                return self.handle_event(input, funcargs)
            elif itype== 0:
                console_log("INFO","\n\nDTMF input: %s\n" % input)
            else:
                console_log("INFO","\n\nUnknown input type: %s\n" % itype)
            return None 

        
        num_tries = 0

        session = self.speech_detect.session

        console_log("debug", "setting dtmf callback\n")
        session.setDTMFCallback(dtmf_handler, "")
        console_log("debug", "calling getDigits\n")
            
        console_log("debug", "starting run() while loop\n")        
        while (session.ready() and 
               num_tries < self.max_tries and
               len(self.detected_phrases) < self.required_phrases and
               not self.failed):
            console_log("debug", "top of run() while loop\n")        
            session.collectDigits(self.wait_time)
            num_tries += 1

        console_log("debug", "while loop finished\n")
        return self.detected_phrases

    def handle_event(self, event, funcargs):
        """
        when the dtmf handler receives an event, it calls back
        this method.  event is a dictionary with subdictionaries ..

        Example 1
        =========

        {'body': None, 'headers': {'Speech-Type': 'begin-speaking'}}

        Example 2
        =========
        {'body': '<result xmlns='http://www.ietf.org/xml/ns/mrcpv2'
        xmlns:ex='http://www.example.com/example' score='100'
        grammar='session:request1@form-level.store'><interpretation>
        <input mode='speech'>waffles</input></interpretation></result>',
        'headers': {'Speech-Type': 'detected-speech'}}

        This dictionary is constructed in run_dtmf_callback() in
        freeswitch_python.cpp

        """

        # what kind of event?
        headers = event['headers']
        speech_type = headers['Speech-Type']
        if speech_type == "begin-speaking":
            # not sure what to do with this, try returning "stop"
            # so that it might stop playing a sound file once
            # speech has been detected 
            return "stop"
        elif speech_type == "detected-speech":
            # extract the detected phrase. from result
            # BUG: this assumes only ONE interpretation in the xml
            # result.  rest will get igored
            # NOTE: have to wrap everything with str() (at least
            # calls to console_log because otherwise it chokes on
            # unicode strings.
            # TODO: check the score
            body = event['body']
            if not body or len(body) == 0 or body == "(null)":
                # freeswitch returned a completely empty result
                self.failed = True
                # do we want to return stop?  what should we return?
                return "stop"

            dom = minidom.parseString(body)
            phrase = dom.getElementsByTagName(self.grammar.obj_path)[0]
            phrase_text = self.getText(phrase)
            if phrase_text:
                self.detected_phrases.append(str(phrase_text))
                # do we want to return stop?  what should we return?
                return "stop"  
        else:
            raise Exception("Unknown speech event: %s" % speech_type)


    def getText(self, elt):

        """ given an element, get its text.  if there is more than
        one text node child, just append all the text together.
        """

        result = ""
        children = elt.childNodes
        for child in children:
            if child.nodeType == child.TEXT_NODE:
                result += str(child.nodeValue)
        return result

