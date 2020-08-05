#!/usr/bin/python2.7
# -*- coding: utf-8 -*-
#
# s3_auth.py for unit tests in mod_http_cache
# Copyright (C) 2019 Vinadata Corporation (vinadata.vn). All rights reserved.
#
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Initial Developer of the Original Code is Quoc-Bao Nguyen <baonq5@vng.com.vn>
# Portions created by the Initial Developer are Copyright (C)
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Quoc-Bao Nguyen <baonq5@vng.com.vn>
#
# s3_auth.python - Generate signature for AWS Signature version 4 for unit test
#

import base64
import datetime
import hashlib
import hmac
from collections import OrderedDict

import requests
from requests.utils import quote


# hashing methods
def hmac256(key, msg):
    return hmac.new(key, msg.encode('utf-8'), hashlib.sha256).digest()


def hmac256_hex(key, msg):
    return hmac.new(key, msg.encode('utf-8'), hashlib.sha256).hexdigest()


def sha256_hex(msg):
    return hashlib.sha256(msg).hexdigest()


# region is a wildcard value that takes the place of the AWS region value
# as COS doesn't use regions like AWS, this parameter can accept any string
def createSignatureKey(key, date_stamp, region, service):
    keyDate = hmac256(('AWS4' + key).encode('utf-8'), date_stamp)
    keyRegion = hmac256(keyDate, region)
    keyService = hmac256(keyRegion, service)
    keySigning = hmac256(keyService, 'aws4_request')
    return keySigning


def query_string(access_key, date_stamp, time_stamp, region):
    fields = OrderedDict()
    fields["X-Amz-Algorithm"] = "AWS4-HMAC-SHA256"
    fields["X-Amz-Credential"] = access_key + '/' + date_stamp + '/' + region + '/s3/aws4_request'
    fields["X-Amz-Date"] = time_stamp
    fields["X-Amz-Expires"] = "604800"  # in seconds
    fields["X-Amz-SignedHeaders"] = "host"

    queries_string = ''.join("%s=%s&" % (key, val) for (key, val) in fields.iteritems())[:-1]

    return quote(queries_string, safe='&=')


def main():
    access_key = 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'
    secret_key = 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'

    # request elements
    http_method = 'GET'
    region = 'HCM'
    bucket = 'bucket1'
    host = 'stg.example.com'
    endpoint = 'https://' + bucket + "." + host
    object_name = 'document.docx'

    # assemble the standardized request
    time = datetime.datetime.utcnow()
    time_stamp = time.strftime('%Y%m%dT%H%M%SZ')
    date_stamp = time.strftime('%Y%m%d')

    print "time_stamp: " + time_stamp
    print "date_stamp: " + date_stamp

    standardized_query_string = query_string(access_key, date_stamp, time_stamp, region)
    print 'standardized_query_string: \n' + standardized_query_string

    standardized_request = (http_method + '\n' +
                            '/' + object_name + '\n' +
                            standardized_query_string + '\n' +
                            'host:' + bucket + '.' + host + '\n\n' +
                            'host' + '\n' +
                            'UNSIGNED-PAYLOAD')

    print 'standardized_request: ' + hashlib.sha256(standardized_request).hexdigest()

    print "\nStandardized request:\n" + standardized_request

    # assemble string-to-sign
    string_to_sign = ('AWS4-HMAC-SHA256' + '\n' +
                      time_stamp + '\n' +
                      date_stamp + '/' + region + '/s3/aws4_request' + '\n' +
                      sha256_hex(standardized_request))

    print "\nString to Sign:\n" + string_to_sign.replace('\n', "\\n")

    # generate the signature
    signature_key = createSignatureKey(secret_key, date_stamp, region, 's3')

    print 'signature_key: ' + base64.b64encode(signature_key)
    # signature = hmac.new(signature_key, sts.encode('utf-8'), hashlib.sha256).hexdigest()
    signature = hmac256_hex(signature_key, string_to_sign)

    print 'signature: ' + signature
    # create and send the request
    # the 'requests' package automatically adds the required 'host' header

    request_url = (endpoint + '/' +
                   object_name + '?' +
                   standardized_query_string +
                   '&X-Amz-Signature=' +
                   signature)

    print '\nRequest URL:\n' + request_url

    request = requests.get(request_url)

    print '\nResponse code: %d\n' % request.status_code
    # print '\nResponse code: %s\n' % request.content
    # print request.text


if __name__ == "__main__":
    main()
