#!/usr/bin/env python

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


from twisted.internet import reactor, defer
from twisted.internet.protocol import ClientFactory
import freepy


class FsHelper(ClientFactory):

    def __init__(self, host=None, passwd=None, port=None):
        if host:
            self.host = host
        if passwd:
            self.passwd = passwd
        if port:
            self.port = port

        self.freepyd = None
        self.connection_deferred = None

    def reset(self):
        self.freepyd = None
        self.connection_deferred = None

    def connect(self):

        if self.freepyd:
            # if we have a protocol object, we are connected (since we always
            # null it upon any disconnection)
            return defer.succeed("Connected")

        if self.connection_deferred:
            # we are already connecting, return existing dfrd
            return self.connection_deferred

        self.connection_deferred = defer.Deferred()
        self.connection_deferred.addCallback(self.dologin)
        self.connection_deferred.addErrback(self.generalError)
        print "freepy connecting to %s:%s" % (self.host, self.port)
        reactor.connectTCP(self.host, self.port, self)
        return self.connection_deferred

    def conncb(self, freepyd):
        self.freepyd = freepyd
        deferred2callback = self.connection_deferred
        self.connection_deferred = None
        deferred2callback.callback("Connected")

    def generalError(self, failure):
        print "General error: %s" % failure
        return failure

    def startedConnecting(self, connector):
        pass

    def buildProtocol(self, addr):
        return freepy.FreepyDispatcher(self.conncb, self.discocb)

    def clientConnectionLost(self, connector, reason):
        print "clientConnectionLost! conn=%s, reason=%s" % (connector,
                                                            reason)
        self.connection_deferred = None
        self.freepyd = None

    def clientConnectionFailed(self, connector, reason):
        print "clientConnectionFailed! conn=%s, reason=%s" % (connector,
                                                              reason)
        self.freepyd = None
        deferred2callback = self.connection_deferred
        self.connection_deferred = None
        deferred2callback.errback(reason)

    def discocb(self, reason):
        print "disconnected.  reason: %s" % reason
        self.freepyd = None

    def dologin(self, connectmsg):
        return self.freepyd.login(self.passwd)

    def originate(self, party2dial, dest_ext_app, bgapi=True):
        """
        party2dial - the first argument to the originate command,
                     eg, sofia/profile_name/1234@domain.com
        dest_ext_app - the second argument to the originate command,
                       eg, &park() or 4761
        returns - a deferred that will be called back with a result
                  like:

                  ([(True, 'Reply-Text: +OK Job-UUID: d07ad7de-2406-11dc-aea3-e3b2e56b7a2c')],)

        """

        def originate_inner(ignored):
            deferreds = []
            deferred = self.freepyd.originate(party2dial,
                                              dest_ext_app,
                                              bgapi)
            return deferred

        d = self.connect()
        d.addCallback(originate_inner)
        return d

    def dialconf(self, people2dial, conf_name, bgapi=True):
        """
        conf_name - name of conf TODO: change to match db
        people2dial - an array of dictionaries:
                     'name': name
                     'number': number
        returns - a deferred that will be called back with a result
                  like:

                  ([(True, 'Reply-Text: +OK Job-UUID: d07ad7de-2406-11dc-aea3-e3b2e56b7a2c')],)

                  Its a bit ugly because its a deferred list callback.

        """

        def dialconf_inner(ignored):
            deferreds = []
            for person2dial in people2dial:
                sofia_url = person2dial['number']
                deferred = self.freepyd.confdialout(conf_name,
                                                    sofia_url,
                                                    bgapi)
                deferreds.append(deferred)
            return defer.DeferredList(deferreds)

        d = self.connect()
        d.addCallback(dialconf_inner)
        return d

    def listconf(self, conf_name):
        """
        conf_name - name of conf
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def listconf_inner(ignored):
            deferred = self.freepyd.listconf(conf_name)
            return deferred

        d = self.connect()
        d.addCallback(listconf_inner)
        return d

    def confkick(self, member_id, conf_name, bgapi=True):
        """
        conf_name - name of conf
        member_id - member id of user to kick, eg, "7"
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def confkick_inner(ignored):
            #if type(member_id) == type(""):
            #    member_id = int(member_id)
            deferred = self.freepyd.confkick(member_id, conf_name, bgapi)
            return deferred

        d = self.connect()
        d.addCallback(confkick_inner)
        return d

    def confdtmf(self, member_id, conf_name, dtmf, bgapi=True):
        """
        Send dtmf(s) to a conference
        conf_name - name of conf
        member_id - member id of user to kick, eg, "7", or "all"
        dtmf - a single dtmf or a string of dtms, eg "1" or "123"
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def confdtmf_inner(ignored):
            print "confdtmf_inner called"
            deferred = self.freepyd.confdtmf(member_id, conf_name, dtmf, bgapi)
            return deferred

        d = self.connect()
        d.addCallback(confdtmf_inner)
        return d

    def confsay(self, conf_name, text2speak, bgapi=True):
        """
        conf_name - name of conf
        text2speak - text to speak
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def confsay_inner(ignored):
            deferred = self.freepyd.confsay(conf_name, text2speak, bgapi)
            return deferred

        d = self.connect()
        d.addCallback(confsay_inner)
        return d

    def confplay(self, conf_name, snd_url, bgapi=True):
        """
        conf_name - name of conf
        snd_url - url to sound file
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def confplay_inner(ignored):
            deferred = self.freepyd.confplay(conf_name, snd_url, bgapi)
            return deferred

        d = self.connect()
        d.addCallback(confplay_inner)
        return d

    def confstop(self, conf_name, bgapi=True):
        """
        stop playback of all sounds

        conf_name - name of conf
        returns - a deferred that will be called back with a result
                  like:

                  TODO: add this
        """
        def confstop_inner(ignored):
            deferred = self.freepyd.confstop(conf_name, bgapi)
            return deferred

        d = self.connect()
        d.addCallback(confstop_inner)
        return d

    def showchannels(self, bgapi=True):

        def showchannels_inner(ignored):
            df = self.freepyd.showchannels(bgapi)
            return df

        d = self.connect()
        d.addCallback(showchannels_inner)
        return d

    def killchan(self, uuid, bgapi=True):

        def killchan_inner(ignored):
            df = self.freepyd.killchan(uuid, bgapi)
            return df

        d = self.connect()
        d.addCallback(killchan_inner)
        return d

    def broadcast(self, uuid, path, legs="both", bgapi=True):
        """
        @legs - one of the following strings: aleg|bleg|both
        """
        def broadcast_inner(ignored):
            df = self.freepyd.broadcast(uuid, path, legs, bgapi)
            return df

        d = self.connect()
        d.addCallback(broadcast_inner)
        return d

    def transfer(self, uuid, dest_ext, legs="-both", bgapi=True):
        """
        @legs -bleg|-both
        """
        def transfer_inner(ignored):
            df = self.freepyd.transfer(uuid, dest_ext, legs, bgapi)
            return df

        d = self.connect()
        d.addCallback(transfer_inner)
        return d

    def sofia_profile_restart(self, profile_name, bgapi=True):

        def sofia_profile_restart_inner(ignored):
            df = self.freepyd.sofia_profile_restart(profile_name,
                                                    bgapi)
            return df

        d = self.connect()
        d.addCallback(sofia_profile_restart_inner)
        return d

    def sofia_status_profile(self, profile_name, bgapi=True):

        def sofia_status_profile_inner(ignored):
            df = self.freepyd.sofia_status_profile(profile_name, bgapi)
            return df

        d = self.connect()
        d.addCallback(sofia_status_profile_inner)
        return d


class FsHelperTest:
    def __init__(self, fshelper):
        self.fshelper = fshelper
        pass

    def test_dialconf(self):

        # the following parties will be dialed out from the conference
        # called "freeswitch" on the local freeswitch instance.
        # one party is actually another conference, just to make
        # the example more confusing.
        people2dial = [
            {
                'name': 'freeswitch',
                'number': '888@conference.freeswitch.org'
            },
            {
                'name': 'mouselike',
                'number': ' 904@mouselike.org'
            }
        ]
        d = self.fshelper.dialconf(people2dial, "freeswitch", bgapi=False)
        def failed(error):
            print "Failed to dial users!"
            reactor.stop()
            return error
        d.addErrback(failed)
        def worked(*args):
            print "Worked! Dialed user result: %s" % str(args)
        d.addCallback(worked)
        return d

    def test_listconf(self):

        d = self.fshelper.listconf("freeswitch")
        def failed(failure):
            print "Failed to list users!"
            reactor.stop()
            return failure
        d.addErrback(failed)
        def worked(*args):
            print "List of users in conf: %s" % str(args)
            return args[0]
        d.addCallback(worked)
        return d

    def test_confkick(self, member_id="6", conf_name="freeswitch"):

        d = self.fshelper.confkick(member_id, conf_name)
        def failed(failure):
            print "Failed to kick user!"
            reactor.stop()
            return failure
        d.addErrback(failed)
        def worked(*args):
            print "Kicked user from conf, result: %s" % str(args)
        d.addCallback(worked)


def test1():
    kick_everyone = False
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)
    fsht = FsHelperTest(fshelper)
    fsht.test_dialconf()
    d = fsht.test_listconf()
    def kickeveryone(members):
        print "Kickeveryone called w/ %s (type: %s)" % (members,
                                                        type(members))
        for member in members:
            fsht.test_confkick(member.member_id)

    def failed(failure):
        print "failed: %s" % str(failure)
        reactor.stop()
    if kick_everyone:
        d.addCallback(kickeveryone)
        d.addErrback(failed)
    #fsht.test_confkick()
    #d = fshelper.connect()
    #def connected(*args):
    #    fsht.test_dialconf()
    #    fsht.test_listconf()
    #d.addCallback(connected)
    reactor.run()

def test2():
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)
    fshelper.sofia_profile_restart("mydomain.com")
    reactor.run()

def test3():
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)
    print "Calling originate.."
    party2dial="sofia/foo/600@192.168.1.202:5080"
    d = fshelper.originate(party2dial=party2dial,
                           dest_ext_app="101",
                           bgapi=True)

    def worked(result):
        print "Originate succeeded: %s" % result
        reactor.stop()

    def failed(failure):
        print "failed: %s" % str(failure)
        reactor.stop()

    d.addCallback(worked)
    d.addErrback(failed)
    reactor.run()


def test4():
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)

    def worked(result):
        print "Originate succeeded: %s" % result
        #reactor.stop()

    def failed(failure):
        print "failed: %s" % str(failure)
        #reactor.stop()


    dest_ext_app = "101"
    party2dial="sofia/foo/600@192.168.1.202:5080"
    d = fshelper.originate(party2dial=party2dial,
                           dest_ext_app=dest_ext_app,
                           bgapi=True)
    d.addCallback(worked)
    d.addErrback(failed)
    party2dial="sofia/foo/someone@bar.com"
    d2 = fshelper.originate(party2dial=party2dial,
                            dest_ext_app=dest_ext_app,
                            bgapi=True)

    d2.addCallback(worked)
    d2.addErrback(failed)
    reactor.run()


def test5():
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)

    def worked(result):
        print "Originate succeeded: %s" % result
        #reactor.stop()

    def failed(failure):
        print "failed: %s" % str(failure)
        #reactor.stop()

    for i in xrange(20):
        party2dial = "sofia/foo/600@192.168.1.202:5080"
        d = fshelper.originate(party2dial=party2dial,
                               dest_ext_app="700",
                               bgapi=True)
        d.addCallback(worked)
        d.addErrback(failed)

    reactor.run()


def test6():
    """
    show channels for a given sofia profile
    """
    fshelper = FsHelper(host="127.0.0.1",
                        passwd="ClueCon",
                        port=8021)
    from wikipbx import channelsutil
    def show_chanels(raw_xml):
        print raw_xml

    def failure(failure):
        print failure

    d = fshelper.showchannels(bgapi=False)
    d.addCallback(show_chanels)
    d.addErrback(failure)
    reactor.run()


if __name__ == "__main__":
    #test1()
    #test2()
    #test3()
    test4()
    #test5()
