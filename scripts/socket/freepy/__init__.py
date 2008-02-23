""" 
FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>

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
Anthony Minessale II <anthmct@yahoo.com>
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
import freepy.globals

"""
freepy library -- connect to freeswitch mod_socket_event via python/twisted

All commands currently use api instead of bgapi.  For the networking model
used (twisted), this seems to work well and is simpler.

"""

class FreepyDispatcher(LineReceiver):

    def __init__(self, conncb, discocb=None):
        self.delimiter='\n' # parent class uses this 
        self.conncb=conncb
        self.discocb=discocb
        self.requestq = Queue() # queue of pending requests
        self.active_request = None # the current active (de-queued) request

    def connectionMade(self):
        print "Connection made"
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
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        if freepy.globals.DEBUG_ON:
            print msg
        return req.getDeferred()

    def confdialout(self, conf_name, sofia_url, bgapi=True):
        """
        Instruct conference to join a particular user via dialout
        @param conf_name - the name of the conference (arbitrary)
        @param party2dial - a freeswitch sofia url, eg, sofia/mydomain.com/foo@bar.com
        @return - a deferred that will be called back with a string like:
                  Reply-Text: +OK Job-UUID: 4d410a8e-2409-11dc-99bf-a5e17fab9c65


        """
        if bgapi == True:
            msg = "bgapi conference %s dial %s" % (conf_name,
                                                   sofia_url)
            req = request.BgDialoutRequest()            
        else:
            msg = "api conference %s dial %s" % (conf_name,
                                                 sofia_url)
            req = request.DialoutRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def originate(self, party2dial, dest_ext_app, bgapi=True):

        if bgapi == True:
            msg = "bgapi originate %s %s" % (party2dial,
                                             dest_ext_app)
            req = request.BgDialoutRequest()            
        else:
            msg = "api originate %s %s" % (party2dial,
                                           dest_ext_app)
            req = request.DialoutRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def listconf(self, conf_name):
        """
        List users in a conf
        @param conf_name - the name of the conference (arbitrary)
        @return - a deferred that will be called back with an array
                  of models.ConfMember instances
        """
        msg = "api conference %s list" % (conf_name)
        req = request.ListConfRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()
        

    def confkick(self, member_id, conf_name, bgapi=False):
        """
        Kick member_id from conf
        conf_name - name of conf
        member_id - member id of user to kick, eg, "7"
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        if bgapi == True:
            msg = "bgapi conference %s kick %s" % (conf_name, member_id)
            req = request.BgConfKickRequest()
        else:
            msg = "api conference %s kick %s" % (conf_name, member_id)
            req = request.ConfKickRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def confdtmf(self, member_id, conf_name, dtmf, bgapi=False):
        """
        Send dtmf to member_id or to all members
        conf_name - name of conf
        member_id - member id of user to kick, eg, "7"
        dtmf - a single dtmf or a string of dtms, eg "1" or "123"        
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        print "confdtmf called"        
        if bgapi == True:
            msg = "bgapi conference %s dtmf %s %s" % \
                  (conf_name, member_id, dtmf)
            req = request.BgApiRequest()
        else:
            msg = "api conference %s dtmf %s %s" % \
                  (conf_name, member_id, dtmf)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def confsay(self, conf_name, text2speak, bgapi=False):
        """
        Speak text all members
        conf_name - name of conf
        dtmf - text to speak
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        if bgapi == True:
            msg = "bgapi conference %s say %s" % \
                  (conf_name, text2speak)
            req = request.BgApiRequest()
        else:
            msg = "api conference %s say %s" % \
                  (conf_name, text2speak)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def confplay(self, conf_name, snd_url, bgapi=False):
        """
        Play a file to all members
        conf_name - name of conf
        dtmf - text to speak
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        if bgapi == True:
            msg = "bgapi conference %s play %s" % \
                  (conf_name, snd_url)
            req = request.BgApiRequest()
        else:
            msg = "api conference %s play %s" % \
                  (conf_name, snd_url)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def confstop(self, conf_name, bgapi=False):
        """
        Stop playback of all sound files
        conf_name - name of conf
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        if bgapi == True:
            msg = "bgapi conference %s stop" % \
                  (conf_name)
            req = request.BgApiRequest()
        else:
            msg = "api conference %s stop" % \
                  (conf_name)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()


    def showchannels(self, bgapi=False):
        """
        Get a list of all live channels on switch
        
        returns - a deferred that will be called back with a result

        <result row_count="2">
          <row row_id="1">
            <uuid>21524b8c-6d19-11dc-9380-357de4a7a612</uuid>
            <created>2007-09-27 11:46:01</created>
            <name>sofia/test/4761</name>
            <state>CS_LOOPBACK</state>
            <cid_name>FreeSWITCH</cid_name>
            <cid_num>0000000000</cid_num>
            <ip_addr></ip_addr>
            <dest>outgoing2endpoint-6207463</dest>
            <application>echo</application>
            <application_data></application_data>
            <read_codec>PCMU</read_codec>
            <read_rate>8000</read_rate>
            <write_codec>PCMU</write_codec>
            <write_rate>8000</write_rate>
          </row>
          ...
        </result>
        """
        if bgapi == True:
            msg = "bgapi show channels as xml"
            req = request.BgApiRequest()            
        else:
            msg = "api show channels as xml"            
            req = request.ApiRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def sofia_status_profile(self, profile_name, bgapi=False):
        # DO NOT USE - TOTALLY BROKEN
        # FS DOES NOT RETURN XML IN THIS CASE
        if bgapi == True:
            msg = "bgapi sofia status profile %s as xml" % (profile_name)
            req = request.BgApiRequest()
        else:
            msg = "api sofia status profile %s as xml" % (profile_name)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()
        
    def sofia_profile_restart(self, sofia_profile_name, bgapi = False):
        if bgapi == True:
            msg = "bgapi sofia profile %s restart" % \
                  (sofia_profile_name)
            req = request.BgApiRequest()
        else:
            msg = "api sofia profile %s restart" % \
                  (sofia_profile_name)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()


    def killchan(self, uuid, bgapi = False):
        if bgapi == True:
            msg = "bgapi killchan %s" % (uuid)
            req = request.BgApiRequest()
        else:
            msg = "api killchan %s" % (uuid)            
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def broadcast(self, uuid, path, legs, bgapi = False):
        if bgapi == True:
            msg = "bgapi broadcast %s %s %s" % (uuid, path, legs)
            req = request.BgApiRequest()
        else:
            msg = "api broadcast %s %s %s" % (uuid, path, legs)            
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()

    def transfer(self, uuid, dest_ext, legs, bgapi = False):
        """
        transfer <uuid> [-bleg|-both] <dest-exten>
        """
        if bgapi == True:
            msg = "bgapi transfer %s %s %s" % (uuid, legs, dest_ext)
            req = request.BgApiRequest()
        else:
            msg = "api transfer %s %s %s" % (uuid, legs, dest_ext)
            req = request.ApiRequest()
        self.requestq.put(req)
        print "sending to fs: %s" % msg
        self.transport.write("%s\n\n" % msg)
        return req.getDeferred()
        
    def lineReceived(self, line):
        if not self.active_request:
            # if no active request, dequeue a new one
            if self.requestq.empty():
                # we are receiving data from fs without an
                # active request pending.  that means that
                # there is a bug in the protocol handler
                # (or possibly in fs)                
                raise Exception("Received line: %s w/ no pending requests" % line)
            self.active_request = self.requestq.get()

        # tell the request to process the line, and tell us
        # if its finished or not.  if its finished, we remove it
        # as the active request so that a new active request will
        # be de-queued.
        finished = self.active_request.process(line)
        if finished == True:
            self.active_request = None

