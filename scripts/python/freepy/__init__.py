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
from freepy.globals import debug

"""
freepy library -- connect to freeswitch mod_socket_event via python/twisted

All commands currently use api instead of bgapi.  For the networking model
used (twisted), this seems to work well and is simpler.

"""

DEBUG_ON = "see globals.py to turn on debugging"

class FreepyDispatcher(LineReceiver):

    def __init__(self, conncb, discocb=None):
        self.delimiter='\n' # parent class uses this 
        self.conncb=conncb
        self.discocb=discocb
        self.requestq = Queue() # queue of pending requests
        self.active_request = None # the current active (de-queued) request

    def connectionMade(self):
        debug("FREEPY: Connection made")
        self.conncb(self)
        
    def connectionLost(self, reason):
        if self.discocb:
            self.discocb(reason)        
        debug("connectionLost: %s" % reason)

    def login(self, passwd):
        """
        send login request
        """
        msg = "auth %s" % passwd
        req = request.LoginRequest()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % msg)
        debug(">> %s" % msg)
        return req.getDeferred()

    def _sendCommand(self, command, bgapi):
        """
        there is a lot of duplication in this object, and as many
        methods as possible should be changed to use this method
        rather than repeating the code
        """
        command = ("bgapi %s" if bgapi else "api %s") % command
        req = (request.BgDialoutRequest if bgapi else
               request.DialoutRequest)()
        self.requestq.put(req)
        self.transport.write("%s\n\n" % command)
        debug(">> %s" % command)
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
        debug(">> %s" % msg)        
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
        debug(">> %s" % msg)                
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
        debug(">> %s" % msg)                        
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
        debug(">> %s" % msg)                        
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
        msg = "conference %s dtmf %s %s" % \
              (conf_name, member_id, dtmf)        
        return self._sendCommand(msg, bgapi)

    def confsay(self, conf_name, text2speak, bgapi=False):
        """
        Speak text all members
        conf_name - name of conf
        dtmf - text to speak
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        msg = "conference %s say %s" % (conf_name, text2speak)        
        return self._sendCommand(msg, bgapi)

    def confplay(self, conf_name, snd_url, bgapi=False):
        """
        Play a file to all members
        conf_name - name of conf
        dtmf - text to speak
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        msg = "conference %s play %s" % (conf_name, snd_url)        
        return self._sendCommand(msg, bgapi)

    def confstop(self, conf_name, bgapi=False):
        """
        Stop playback of all sound files
        conf_name - name of conf
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this        
        """
        msg = "conference %s stop" % (conf_name)        
        return self._sendCommand(msg, bgapi)

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
        msg = "show channels as xml"        
        return self._sendCommand(msg, bgapi)

    def sofia_status_profile(self, profile_name, bgapi=False):
        msg = "sofia status profile %s as xml" % (profile_name)        
        return self._sendCommand(msg, bgapi)

    def sofia_profile_restart(self, sofia_profile_name, bgapi = False):

        msg = "sofia profile %s restart" % (sofia_profile_name)
        return self._sendCommand(msg, bgapi)

    def killchan(self, uuid, bgapi = False):
        return self._sendCommand("uuid_kill %s" % (uuid), bgapi)

    def broadcast(self, uuid, path, legs, bgapi = False):
        msg = "uuid_broadcast %s %s %s" % (uuid, path, legs)
        return self._sendCommand(msg, bgapi)
    
    def transfer(self, uuid, dest_ext, legs, bgapi = False):
        """
        transfer <uuid> [-bleg|-both] <dest-exten>
        """
        msg = "uuid_transfer %s %s %s" % (uuid, legs, dest_ext)        
        return self._sendCommand(msg, bgapi)

        
    def lineReceived(self, line):
        debug("<< %s" % line)                                
        if not self.active_request:

            # if no active request pending, we ignore
            # blank lines
            if not line.strip():
                return
            
            # if no active request, dequeue a new one
            if self.requestq.empty():
                # we are receiving non-empty data from fs without an
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

