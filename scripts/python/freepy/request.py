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
from twisted.python.failure import Failure
import time, re
from time import strftime
from Queue import Queue

from freepy import models
import freepy.globals
from freepy.globals import debug

"""
These are response handlers for different types of requests.
It reads the response from freeswitch, and calls back
self.deferred with the result.

The naming could be improved, but here is the translation:

LoginRequest - Response handler for a login request

"""


class FreepyRequest(object):

    def __init__(self):
        self.deferred = defer.Deferred()
        self.response_content = ""
        self.finished = False

    def isRequestFinished(self):
        return self.finished

    def setRequestFinished(self):
        debug("setRequestFinished called.  response_content: %s " %
              self.response_content)
        self.finished = True

    def getDeferred(self):
        return self.deferred

    def callbackDeferred(self, cbval):
        self.deferred.callback(cbval)

    def errbackDeferred(self, result):
        self.deferred.errback(Exception(str(result)))

    def process(self, line):
        """
        processs a line from the fs response.  if the fs response has been
        detected to be finished, then:

        * create an appropriate response based on request type
        * callback deferred with response
        * rturn True to indicate we are finished

        otherwise, if the fs response is incomplete, just buffer the data
        """
        if not line.strip() or len(line.strip()) == 0:
            self._fsm.BlankLine()
            return self.isRequestFinished()

        matchstr = re.compile("auth/request", re.I)
        result = matchstr.search(line)
        if (result != None):
            self._fsm.AuthRequest()
            return self.isRequestFinished()


        matchstr = re.compile("command/reply", re.I)
        result = matchstr.search(line)
        if (result != None):
            self._fsm.CommandReply()
            return self.isRequestFinished()


        matchstr = re.compile("Reply-Text", re.I)
        result = matchstr.search(line)
        if (result != None):
            debug("FREEPY: got Reply-Text")
            fields = line.split(":") # eg, ['Reply-Text','+OK Job-UUID', '882']
            endfields = fields[1:]
            self.response_content = "".join(endfields)
            self._fsm.ReplyText()
            return self.isRequestFinished()

        matchstr = re.compile("Job-UUID", re.I)
        result = matchstr.search(line)
        if (result != None):
            fields = line.split(":") # eg, ['Job-UUID','c9eee07e-508-..']
            endfields = fields[1:]
            # ignore job uuid given on this line, take the one sent
            # in Reply-Text response line
            # self.response_content = "".join(endfields)
            self._fsm.JobUuid()
            return self.isRequestFinished()

        matchstr = re.compile("api/response", re.I)
        result = matchstr.search(line)
        if (result != None):
            self._fsm.ApiResponse()
            return self.isRequestFinished()

        matchstr = re.compile("Content-Length", re.I)
        result = matchstr.search(line)
        if (result != None):
            # line: Content-Length: 34
            self.content_length = int(line.split(":")[1].strip())
            self._fsm.ContentLength()
            return self.isRequestFinished()

        self._fsm.ProcessLine(line)
        return self.isRequestFinished()

    def callOrErrback(self):
        matchstr = re.compile("OK", re.I)
        result = matchstr.search(self.response_content)
        if (result != None):
            self.callbackDeferred(self.response_content)
            return
        self.errbackDeferred(Failure(Exception(self.response_content)))

    def doNothing(self):
        # weird smc issue workaround attempt
        pass


class LoginRequest(FreepyRequest):
    """
    Example success response
    ========================

    lineReceived: Content-Type: auth/request
    lineReceived:
    lineReceived: Content-Type: command/reply
    lineReceived: Reply-Text: +OK accepted
    lineReceived:

    Example failure response
    ========================

    lineReceived: Content-Type: auth/request
    lineReceived:
    lineReceived: Content-Type: command/reply
    lineReceived: Reply-Text: -ERR invalid
    lineReceived:

    """

    def __init__(self):
        super(LoginRequest, self).__init__()
        import loginrequest_sm
        self._fsm = loginrequest_sm.LoginRequest_sm(self)

    def callOrErrback(self):
        matchstr = re.compile("OK", re.I)
        result = matchstr.search(self.response_content)
        if (result != None):
            self.callbackDeferred(self.response_content)
            return
        msg = "Login failed, most likely a bad password"
        self.errbackDeferred(Failure(Exception(msg)))

    def getReplyText(self):
        self.response_content


class BgApiRequest(FreepyRequest):

    """
    Here is one of the 'bgapi requests' this class
    supports:


    linereceived: Content-Type: command/reply
    linereceived: Reply-Text: +OK Job-UUID: 788da080-24e0-11dc-85f6-3d7b12..
    linereceived:

    """
    def __init__(self):
        super(BgApiRequest, self).__init__()
        import bgapirequest_sm
        self._fsm = bgapirequest_sm.BgApiRequest_sm(self)


    def getResponse(self):

        # subclasses may want to parse this into a meaningful
        # object or set of objects (eg, see ListConfRequest)
        # By default, just return accumulated string
        return self.response_content



class ApiRequest(FreepyRequest):

    """
    Here is one of the 'api requests' this class
    supports:

    lineReceived: Content-Type: api/response
    lineReceived: Content-Length: 34
    lineReceived:
    lineReceived: Call Requested: result: [SUCCESS]
    """

    def __init__(self):
        super(ApiRequest, self).__init__()
        import apirequest_sm
        self._fsm = apirequest_sm.ApiRequest_sm(self)
        self.response_content = ""


    def doNothing(self):
        # weird smc issue workaround attempt
        pass

    def add_content(self, line):
        """
        Add content to local buffer
        return - True if finished adding content, False otherwise
        """

        # since the twisted LineReceiver strips off the newline,
        # we need to add it back .. otherwise the Content-length
        # will be off by one
        line += "\n"

        self.response_content += line
        if len(self.response_content) == self.content_length:
            return True
        elif len(self.response_content) > self.content_length:
            return True
        else:
            return False


    def getResponse(self):

        # subclasses may want to parse this into a meaningful
        # object or set of objects (eg, see ListConfRequest)
        # By default, just return accumulated string
        return self.response_content


class DialoutRequest(ApiRequest):
    """
    Example raw dialout response
    ============================

    lineReceived: Content-Type: api/response
    lineReceived: Content-Length: 34
    lineReceived:
    lineReceived: Call Requested: result: [SUCCESS]
    """

    def __init__(self):
        super(DialoutRequest, self).__init__()


class BgDialoutRequest(BgApiRequest):
    def __init__(self):
        super(BgDialoutRequest, self).__init__()


class ConfKickRequest(ApiRequest):
    """
    Example response
    ================


    """

    def __init__(self):
        super(ConfKickRequest, self).__init__()

class BgConfKickRequest(BgApiRequest):
    """
    Example response
    ================


    """

    def __init__(self):
        super(BgConfKickRequest, self).__init__()


class ListConfRequest(ApiRequest):
    """
    Response to request to list conferences:
    ========================================

    lineReceived: Content-Type: api/response
    lineReceived: Content-Length: 233
    lineReceived:
    lineReceived: 2;sofia/mydomain.com/foo@bar.com;e9be6e72-2410-11dc-8daf-7bcec6dda2ae;FreeSWITCH;0000000000;hear|speak;0;0;300
    lineReceived: 1;sofia/mydomain.com/foo2@bar.com;e9be5fcc-2410-11dc-8daf-7bcec6dda2ae;FreeSWITCH;0000000000;hear|speak;0;0;300

    """

    def __init__(self):
        super(ListConfRequest, self).__init__()
        self.conf_members = []

    def add_content(self, line):
        """
        conf not empty example
        ======================
        1;sofia/mydomain.com/888@conference.freeswitch.org;898e6552-24ab-11dc-9df7-9fccd4095451;FreeSWITCH;0000000000;hear|speak;0;0;300

        conf empty example
        ==================
        Conference foo not found
        """

        matchstr = re.compile("not found", re.I)
        result = matchstr.search(line)
        if (result != None):
            # no conf found..
            pass
        else:
            confmember = models.ConfMember(line)
            self.conf_members.append(confmember)

        return super(ListConfRequest, self).add_content(line)

    def getResponse(self):

        # TODO: parse this content into a meaningful
        # 'object' .. though, not sure this is really
        # necessary.   wait till there's a need
        return self.conf_members

