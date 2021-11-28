import os
from freeswitch import *

# HANGUP HOOK
#
# session is a session object
# what is "hangup" or "transfer"
# if you pass an extra arg to setInputCallback then append 'arg' to get that value
# def hangup_hook(session, what, arg):
def hangup_hook(session, what):

	consoleLog("info","hangup hook for %s!!\n\n" % what)
	return


# INPUT CALLBACK
#
# session is a session object
# what is "dtmf" or "event"
# obj is a dtmf object or an event object depending on the 'what' var.
# if you pass an extra arg to setInputCallback then append 'arg' to get that value
# def input_callback(session, what, obj, arg):
def input_callback(session, what, obj):

	if (what == "dtmf"):
		consoleLog("info", what + " " + obj.digit + "\n")
	else:
		consoleLog("info", what + " " + obj.serialize() + "\n")		
	return "pause"

# APPLICATION
#
# default name for apps is "handler" it can be overridden with <modname>::<function>
# session is a session object
# args is all the args passed after the module name
def handler(session, args):

	session.answer()
	session.setHangupHook(hangup_hook)
	session.setInputCallback(input_callback)
	session.execute("playback", session.getVariable("hold_music"))

# FSAPI CALL FROM CLI, DP HTTP etc
#
# default name for python FSAPI is "fsapi" it can be overridden with <modname>::<function>
# stream is a switch_stream, anything written with stream.write() is returned to the caller
# env is a switch_event
# args is all the args passed after the module name
# session is a session object when called from the dial plan or the string "na" when not.
def fsapi(session, stream, env, args):

	stream.write("w00t!\n" + env.serialize())
	

# RUN A FUNCTION IN A THREAD
#
# default name for pyrun is "runtime" it can be overridden with <modname>::<function>
# args is all the args passed after the module name
def runtime(args):

	print args + "\n"

# BIND TO AN XML LOOKUP
#
# default name for binding to an XML lookup is "xml_fetch" it can be overridden with <modname>::<function>
# params a switch_event with all the relevant data about what is being searched for in the XML registry.
#
def xml_fetch(params):

	xml = '''
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<document type="freeswitch/xml">
  <section name="dialplan" description="RE Dial Plan For FreeSWITCH">
    <context name="default">
      <extension name="generated">
        <condition>
         <action application="answer"/>
         <action application="playback" data="${hold_music}"/>
        </condition>
      </extension>
    </context>
  </section>
</document>
'''

	return xml




