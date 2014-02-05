""" 
FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>

Version: MPL 1.1

The contents of this file are subject to the Mozilla Public License Version
1.1 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

The Initial Developer of the Original Code is
Anthony Minessale II <anthm@freeswitch.org>
Portions created by the Initial Developer are Copyright (C)
the Initial Developer. All Rights Reserved.

Contributor(s): Traun Leyden <tleyden@branchcut.com>
"""

import sys

from twisted.internet import reactor, defer
from twisted.protocols.basic import LineReceiver
from twisted.internet.protocol import Protocol, ClientFactory
from twisted.python import failure
import time, re
from time import strftime
from Queue import Queue
from freepy import request

"""
This class connects to freeswitch and listens for
events and calls callback with the events.

Example messages
=================

Content-Length: 675
Content-Type: text/event-xml

<event>
  <header name="force-contact" value="nat-connectile-dysfunction"></header>
  etc..
</event>
Content-Length: 875
Content-Type: text/event-xml

<event>
...
</event>

"""

class FreeswitchEventListener(LineReceiver):

    def __init__(self, conncb, discocb=None):
        self.delimiter='\n' # parent class uses this 
        self.conncb=conncb
        self.discocb=discocb
        self.bufferlines = []
        self.receiving_event = False # state to track if in <event>..</event>
        self.requestq = Queue() # queue of pending requests
        self.active_request = None # the current active (de-queued) request

    def connectionMade(self):
        self.conncb(self)
        
    def connectionLost(self, reason):
        if self.discocb:
            self.discocb(reason)        
        print "connectionLost: %s" % reason


    def login(self, passwd):
        """
        send login request
        """
        msg = "auth %s" % passwd
        req = request.LoginRequest()
        self.active_request = req
        self.transport.write("%s\n\n" % str(msg))
        return req.getDeferred()

    def sniff_events(self, output_type, events):
        """
        @param output_type - eg, xml or plain
        @param events - list of events, eg ['all']
        """
        event_list = " ".join(events)
        msg = "event %s %s" % (output_type, event_list)
        self.transport.write("%s\n\n" % str(msg))

    def sniff_custom_events(self, output_type, events):
        """
        when sniffing custom events, the CUSTOM keyword
        must be present in message
        http://wiki.freeswitch.org/wiki/Event_Socket#event
        
        @param output_type - eg, xml or plain
        @param events - list of events, eg ['all']
        """
        event_list = " ".join(events)
        msg = "event %s CUSTOM %s" % (output_type, event_list)
        self.transport.write("%s\n\n" % msg)

    def sniff_all_events(self, output_type):
        """
        @param output_type - eg, xml or plain
        """
        msg = "event %s all" % output_type
        self.transport.write("%s\n\n" % str(msg))

    def lineReceived(self, line):
        if not self.active_request:
            if line.find("<event>") != -1:
                self.receiving_event = True
            if self.receiving_event:
                self.bufferlines.append(line)
            if line.find("</event>") != -1:
                event_xml_str = "\n".join(self.bufferlines)
                self.eventReceived(event_xml_str)
                self.bufferlines = []
                self.receiving_event = False
        else:
            # we have an active request (seperate state machine)
            # tell the request to process the line, and tell us
            # if its finished or not.  if its finished, we remove it
            # as the active request so that a new active request will
            # be de-queued.
            finished = self.active_request.process(line)
            if finished == True:
                self.active_request = None

    def eventReceived(self, event_xml_str):
        """
        should be overridden by subclasses.
        """
        raise Exception("This is an abstract class, should be overridden "
                        "in a subclass")

class FreeswitchEventListenerFactory(ClientFactory):

    def __init__(self, protoclass, host=None, passwd=None, port=None):
        """
        @param protoclass - a class (not instance) of the protocol
                            should be a subclass of a FreeswitchEventListener
        """

        # dictionary of observers.  key: event name, value: list of observers
        self.event2observer = {}
        
        self.protoclass=protoclass
        
        if host:
            self.host = host
        if passwd:
            self.passwd = passwd
        if port:
            self.port = port        
            
        self.protocol = None
        self.connection_deferred = None
        self.num_attempts = 0

    def addobserver(self, event_name, observer):
        """
        @param event_name, eg "CHANNEL_ANSWER"
        @param observer (instance of object that has an eventReceived() method
        """
        observers = self.event2observer.get(event_name, [])
        observers.append(observer)
        self.event2observer[event_name] = observers

    def dispatch2observers(self, event_name, event_xml_str, event_dom):
        """
        called back by the underlying protocol upon receiving an
        event from freeswitch.  Currently subclasses must explicitly
        call this method from their eventReceived method for observers
        to get the message.  TODO: move this call to FreeswitchEventListener
        and use observer pattern instead of any subclassing.
        """
        observers = self.event2observer.get(event_name, [])
        for observer in observers:
            observer.eventReceived(event_name, event_xml_str, event_dom)
        
    def reset(self):
        self.protocol = None
        self.connection_deferred = None
        
    def connect(self):

        if self.protocol:
            # if we have a protocol object, we are connected (since we always
            # null it upon any disconnection)
            return defer.succeed(self.protocol)

        #if self.connection_deferred:
            # we are already connecting, return existing dfrd
        #    return self.connection_deferred

        # connect and automatically login after connection
        if not self.connection_deferred:
            self.connection_deferred = defer.Deferred()
            self.connection_deferred.addCallback(self.dologin)
            self.connection_deferred.addErrback(self.generalError)
        reactor.connectTCP(self.host, self.port, self)
        return self.connection_deferred

    def conncb(self, protocol):
        self.protocol = protocol
        self.protocol.__dict__["factory"] = self
        deferred2callback = self.connection_deferred
        self.connection_deferred = None
        deferred2callback.callback(self.protocol)


    def generalError(self, failure):
        print "General error: %s" % failure
        return failure

    def startedConnecting(self, connector):
        pass
    
    def buildProtocol(self, addr):
        return self.protoclass(self.conncb, self.discocb)
    
    def clientConnectionLost(self, connector, reason):
        print "clientConnectionLost! conn=%s, reason=%s" % (connector,
                                                            reason)
        self.connection_deferred = None        
        self.protocol = None
    
    def clientConnectionFailed(self, connector, reason):

        if self.num_attempts < 10000:
            self.num_attempts += 1            
            print "Connection to %s:%s refused, retrying attempt #%s in " \
                  "5 seconds" % (self.host, self.port, self.num_attempts)
            return reactor.callLater(5, self.connect)
        else:
            print "clientConnectionFailed! conn=%s, reason=%s" % (connector,
                                                                  reason)
            print ("Retry attempts exhausted, total attempts: %s" % 
                  self.num_attempts)
            deferred2callback = self.connection_deferred
            deferred2callback.errback(reason)

    def discocb(self, reason):
        print "disconnected.  reason: %s" % reason
        self.protocol = None

    def dologin(self, connectmsg):
        return self.protocol.login(self.passwd)
        

def test1():
    fel = FreeswitchEventListener
    factory = FreeswitchEventListenerFactory(protoclass=fel,
                                             host="127.0.0.1",
                                             port=8021,
                                             passwd="ClueCon")

    def connected(result):
        print "We connected, result: %s" % result
        events=['sofia::register','sofia::expire']
        factory.protocol.sniff_custom_events(output_type="xml", events=events)
        #factory.protocol.sniff_all_events(output_type="xml")
    
    def failure(failure):
        print "Failed to connect: %s" % failure
        
    d = factory.connect()
    d.addCallbacks(connected, failure)
    d.addErrback(failure)

    reactor.run()    
    
if __name__=="__main__":
    test1()

