/*
 * Verto HTML5/Javascript Telephony Signaling and Control Protocol Stack for FreeSWITCH
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Verto HTML5/Javascript Telephony Signaling and Control Protocol Stack for FreeSWITCH
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * jquery.FSRTC.js - WebRTC Glue code
 *
 */

(function($) {

    // Find the line in sdpLines that starts with |prefix|, and, if specified,
    // contains |substr| (case-insensitive search).
    function findLine(sdpLines, prefix, substr) {
        return findLineInRange(sdpLines, 0, -1, prefix, substr);
    }

    // Find the line in sdpLines[startLine...endLine - 1] that starts with |prefix|
    // and, if specified, contains |substr| (case-insensitive search).
    function findLineInRange(sdpLines, startLine, endLine, prefix, substr) {
        var realEndLine = (endLine != -1) ? endLine : sdpLines.length;
        for (var i = startLine; i < realEndLine; ++i) {
            if (sdpLines[i].indexOf(prefix) === 0) {
                if (!substr || sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {
                    return i;
                }
            }
        }
        return null;
    }

    // Gets the codec payload type from an a=rtpmap:X line.
    function getCodecPayloadType(sdpLine) {
        var pattern = new RegExp('a=rtpmap:(\\d+) \\w+\\/\\d+');
        var result = sdpLine.match(pattern);
        return (result && result.length == 2) ? result[1] : null;
    }

    // Returns a new m= line with the specified codec as the first one.
    function setDefaultCodec(mLine, payload) {
        var elements = mLine.split(' ');
        var newLine = [];
        var index = 0;
        for (var i = 0; i < elements.length; i++) {
            if (index === 3) { // Format of media starts from the fourth.
                newLine[index++] = payload; // Put target payload to the first.
            }
            if (elements[i] !== payload) newLine[index++] = elements[i];
        }
        return newLine.join(' ');
    }

    $.FSRTC = function(options) {
        this.options = $.extend({
            useVideo: null,
            useStereo: false,
            userData: null,
	    localVideo: null,
	    screenShare: false,
	    useCamera: "any",
            iceServers: false,
            videoParams: {},
            audioParams: {},
            callbacks: {
                onICEComplete: function() {},
                onICE: function() {},
                onOfferSDP: function() {}
            },
      useStream: null,
        }, options);

	this.audioEnabled = true;
	this.videoEnabled = true;


        this.mediaData = {
            SDP: null,
            profile: {},
            candidateList: []
        };

	this.constraints = {
	    offerToReceiveAudio: this.options.useSpeak === "none" ? false : true,
	    offerToReceiveVideo: this.options.useVideo ? true : false,
	};

        if (self.options.useVideo) {
            self.options.useVideo.style.display = 'none';
        }

        setCompat();
        checkCompat();
    };

    $.FSRTC.validRes = [];

    $.FSRTC.prototype.useVideo = function(obj, local) {
        var self = this;

        if (obj) {
            self.options.useVideo = obj;
	    self.options.localVideo = local;
	    self.constraints.offerToReceiveVideo = true;
        } else {
            self.options.useVideo = null;
	    self.options.localVideo = null;
	    self.constraints.offerToReceiveVideo = false;
        }

        if (self.options.useVideo) {
            self.options.useVideo.style.display = 'none';
        }
    };

    $.FSRTC.prototype.useStereo = function(on) {
        var self = this;
        self.options.useStereo = on;
    };

    // Sets Opus in stereo if stereo is enabled, by adding the stereo=1 fmtp param.
    $.FSRTC.prototype.stereoHack = function(sdp) {
        var self = this;

        if (!self.options.useStereo) {
            return sdp;
        }

        var sdpLines = sdp.split('\r\n');

        // Find opus payload.
        var opusIndex = findLine(sdpLines, 'a=rtpmap', 'opus/48000'), opusPayload;

        if (!opusIndex) {
	    return sdp;
	} else {
            opusPayload = getCodecPayloadType(sdpLines[opusIndex]);
        }

        // Find the payload in fmtp line.
        var fmtpLineIndex = findLine(sdpLines, 'a=fmtp:' + opusPayload.toString());

        if (fmtpLineIndex === null) {
	    // create an fmtp line
	    sdpLines[opusIndex] = sdpLines[opusIndex] + '\r\na=fmtp:' + opusPayload.toString() + " stereo=1; sprop-stereo=1"
	} else {
            // Append stereo=1 to fmtp line.
            sdpLines[fmtpLineIndex] = sdpLines[fmtpLineIndex].concat('; stereo=1; sprop-stereo=1');
	}

        sdp = sdpLines.join('\r\n');
        return sdp;
    };

    function setCompat() {
    }

    function checkCompat() {
        return true;
    }

    function onStreamError(self, e) {
        console.log('There has been a problem retrieving the streams - did you allow access? Check Device Resolution', e);
        doCallback(self, "onError", e);
    }

    function onStreamSuccess(self, stream) {
        console.log("Stream Success");
        doCallback(self, "onStream", stream);
    }

    function onRemoteStreamSuccess(self, stream) {
      console.log("Remote Stream Success");
      doCallback(self, "onRemoteStream", stream);
    }

    function onICE(self, candidate) {
        self.mediaData.candidate = candidate;
        self.mediaData.candidateList.push(self.mediaData.candidate);

        doCallback(self, "onICE");
    }

    function doCallback(self, func, arg) {
        if (func in self.options.callbacks) {
            self.options.callbacks[func](self, arg);
        }
    }

    function onICEComplete(self, candidate) {
        console.log("ICE Complete");
        doCallback(self, "onICEComplete");
    }

    function onChannelError(self, e) {
        console.error("Channel Error", e);
        doCallback(self, "onError", e);
    }

    function onICESDP(self, sdp) {
        self.mediaData.SDP = self.stereoHack(sdp.sdp);
        console.log("ICE SDP");
        doCallback(self, "onICESDP");
    }

    function onAnswerSDP(self, sdp) {
        self.answer.SDP = self.stereoHack(sdp.sdp);
        console.log("ICE ANSWER SDP");
        doCallback(self, "onAnswerSDP", self.answer.SDP);
    }

    function onMessage(self, msg) {
        console.log("Message");
        doCallback(self, "onICESDP", msg);
    }

    FSRTCattachMediaStream = function(element, stream) {
        if (typeof element.srcObject !== 'undefined') {
	    element.srcObject = stream;
	} else {
	    console.error('Error attaching stream to element.');
	}
    }

    function onRemoteStream(self, stream) {
        if (self.options.useVideo) {
            self.options.useVideo.style.display = 'block';

	    // Hacks for Mobile Safari
	    var iOS = ['iPad', 'iPhone', 'iPod'].indexOf(navigator.platform) >= 0;

	    if (iOS) {
		self.options.useVideo.setAttribute("playsinline", true);
	    }
        }

        var element = self.options.useAudio;
        console.log("REMOTE STREAM", stream, element);

	FSRTCattachMediaStream(element, stream);



        //self.options.useAudio.play();
        self.remoteStream = stream;
        onRemoteStreamSuccess(self, stream);
    }

    function onOfferSDP(self, sdp) {
        self.mediaData.SDP = self.stereoHack(sdp.sdp);
        console.log("Offer SDP");
        doCallback(self, "onOfferSDP");
    }

    $.FSRTC.prototype.answer = function(sdp, onSuccess, onError) {
        this.peer.addAnswerSDP({
            type: "answer",
            sdp: sdp
        },
        onSuccess, onError);
    };

    $.FSRTC.prototype.stopPeer = function() {
        if (self.peer) {
            console.log("stopping peer");
            self.peer.stop();
        }
    }

    $.FSRTC.prototype.stop = function() {
        var self = this;

        if (self.options.useVideo) {
            self.options.useVideo.style.display = 'none';
            self.options.useVideo['src'] = '';
        }

        if (self.localStream && !self.options.useStream) {
            var tracks = self.localStream.getTracks();
            console.log("Stopping localStream tracks:", tracks);
            tracks.forEach(function(track){ track.stop() })
            self.localStream = null;
        }

        if (self.options.localVideo) {
            deactivateLocalVideo(self.options.localVideo);
        }

        if (self.options.localVideoStream && !self.options.useStream) {
            var tracks = self.options.localVideoStream.getTracks();
            console.log("Stopping localVideoStream tracks:", tracks);
            tracks.forEach(function(track){ track.stop() })
        }

        if (self.peer) {
            console.log("stopping peer");
            self.peer.stop();
        }
    };

    $.FSRTC.prototype.getMute = function() {
	var self = this;
	return self.audioEnabled;
    }

    $.FSRTC.prototype.setMute = function(what) {
	var self = this;
    if (!self.localStream) {
        return false;
    }
	var audioTracks = self.localStream.getAudioTracks();

	for (var i = 0, len = audioTracks.length; i < len; i++ ) {
	    switch(what) {
	    case "on":
		audioTracks[i].enabled = true;
		break;
	    case "off":
		audioTracks[i].enabled = false;
		break;
	    case "toggle":
		audioTracks[i].enabled = !audioTracks[i].enabled;
	    default:
		break;
	    }

	    self.audioEnabled = audioTracks[i].enabled;
	}

	return !self.audioEnabled;
    }

    $.FSRTC.prototype.getVideoMute = function() {
	var self = this;
	return self.videoEnabled;
    }

    $.FSRTC.prototype.setVideoMute = function(what) {
	var self = this;
    if (!self.localStream) {
        return false;
    }
	var videoTracks = self.localStream.getVideoTracks();

	for (var i = 0, len = videoTracks.length; i < len; i++ ) {
	    switch(what) {
	    case "on":
		videoTracks[i].enabled = true;
		break;
	    case "off":
		videoTracks[i].enabled = false;
		break;
	    case "toggle":
		videoTracks[i].enabled = !videoTracks[i].enabled;
	    default:
		break;
	    }

	    self.videoEnabled = videoTracks[i].enabled;
	}

	return !self.videoEnabled;
    }

    $.FSRTC.prototype.createAnswer = function(params) {
        var self = this;
        self.type = "answer";
        self.remoteSDP = params.sdp;
        console.debug("inbound sdp: ", params.sdp);

        function onSuccess(stream) {
            self.localStream = stream;

            self.peer = FSRTCPeerConnection({
                type: self.type,
                attachStream: self.localStream,
                onICE: function(candidate) {
                    return onICE(self, candidate);
                },
                onICEComplete: function() {
                    return onICEComplete(self);
                },
                onRemoteStream: function(stream) {
                    return onRemoteStream(self, stream);
                },
                onICESDP: function(sdp) {
                    return onICESDP(self, sdp);
                },
                onChannelError: function(e) {
                    return onChannelError(self, e);
                },
                constraints: self.constraints,
                iceServers: self.options.iceServers,
                offerSDP: {
                    type: "offer",
                    sdp: self.remoteSDP
                },
                turnServer: self.options.turnServer
            });

            onStreamSuccess(self, stream);
        }

        function onError(e) {
            onStreamError(self, e);
        }

	var mediaParams = getMediaParams(self);

	console.log("Audio constraints", mediaParams.audio);
	console.log("Video constraints", mediaParams.video);

    if (self.options.useVideo && self.options.localVideo && !self.options.useStream) {
            getUserMedia({
		constraints: {
                    audio: false,
                    video: { deviceId: params.useCamera },
		},
        localVideo: self.options.localVideo,
        useCameraLabel: self.options.useCameraLabel,
        useMicLabel: self.options.useMicLabel,
		onsuccess: function(e) {self.options.localVideoStream = e; console.log("local video ready");},
		onerror: function(e) {console.error("local video error!", e);}
            });
	}

    if (self.options.useStream) {
      if (self.options.useVideo) {
        self.options.localVideoStream = self.options.useStream;
        if (self.options.localVideo) {
          activateLocalVideo(self.options.localVideo, self.options.useStream);
        }
      }
      onSuccess(self.options.useStream);
    }
    else {
      getUserMedia({
        constraints: {
          audio: mediaParams.audio,
          video: mediaParams.video
        },
        video: mediaParams.useVideo,
        useCameraLabel: self.options.useCameraLabel,
        useMicLabel: self.options.useMicLabel,
        onsuccess: onSuccess,
        onerror: onError
      });
    }

    };

    function getMediaParams(obj) {

	var audio;

	if (obj.options.useMic && obj.options.useMic === "none") {
	    console.log("Microphone Disabled");
	    audio = false;
	} else if (obj.options.videoParams && obj.options.screenShare) {//obj.options.videoParams.chromeMediaSource == 'desktop') {
	    console.error("SCREEN SHARE", obj.options.videoParams);
	    audio = false;
	} else {
	    audio = {
	    };

	    if (obj.options.audioParams) {
	        audio = obj.options.audioParams;
            }

	    if (obj.options.useMic !== "any") {
		//audio.optional = [{sourceId: obj.options.useMic}];
		audio.deviceId = assignMediaIdToConstraint(obj.options.useMic);
	    }
	}

    if (obj.options.useVideo && obj.options.localVideo && !obj.options.useStream) {
            getUserMedia({
		constraints: {
                    audio: false,
                    video: { deviceId: obj.options.useCamera },
		},
        localVideo: obj.options.localVideo,
        useCameraLabel: obj.options.useCameraLabel,
        useMicLabel: obj.options.useMicLabel,
		onsuccess: function(e) {obj.options.localVideoStream = e; console.log("local video ready");},
		onerror: function(e) {console.error("local video error!", e); }
            });
	}

	var video = {};
	var bestFrameRate = obj.options.videoParams.vertoBestFrameRate;
	var minFrameRate = obj.options.videoParams.minFrameRate || 15;
	delete obj.options.videoParams.vertoBestFrameRate;

	if (obj.options.screenShare) {
	    if (!obj.options.useCamera && !!navigator.mozGetUserMedia) {
		//This is an issue, only FireFox needs to ask this additional question if its screen or window we need a better way
		var dowin = window.confirm("Do you want to share an application window?  If not you can share an entire screen.");

		video = {
		    width: {min: obj.options.videoParams.minWidth, max: obj.options.videoParams.maxWidth},
		    height: {min: obj.options.videoParams.minHeight, max: obj.options.videoParams.maxHeight},
		    mediaSource: dowin ? "window" : "screen"
		}
	    } else {
		var opt = [];
		if (obj.options.useCamera) {
		    opt.push({sourceId: obj.options.useCamera});
		}

		if (bestFrameRate) {
		    opt.push({minFrameRate: bestFrameRate});
		    opt.push({maxFrameRate: bestFrameRate});
		}

		video = {
		    mandatory: obj.options.videoParams,
		    optional: opt
		};
	    }
	} else {

	    video = {
		//mandatory: obj.options.videoParams,
		width: {min: obj.options.videoParams.minWidth, max: obj.options.videoParams.maxWidth},
		height: {min: obj.options.videoParams.minHeight, max: obj.options.videoParams.maxHeight}
	    };



	    var useVideo = obj.options.useVideo;

	    if (useVideo && obj.options.useCamera && obj.options.useCamera !== "none") {
		//if (!video.optional) {
		//video.optional = [];
		//}


		if (obj.options.useCamera !== "any") {
		    //video.optional.push({sourceId: obj.options.useCamera});
        video = assignMediaIdToConstraint(obj.options.useCamera, video);
		}

		if (bestFrameRate) {
		    //video.optional.push({minFrameRate: bestFrameRate});
		    //video.optional.push({maxFrameRate: bestFrameRate});
		    video.frameRate = {ideal: bestFrameRate, min: minFrameRate, max: 30};
		}

	    } else {
		console.log("Camera Disabled");
		video = false;
		useVideo = false;
	    }
	}

	return {audio: audio, video: video, useVideo: useVideo};
    }

    $.FSRTC.prototype.call = function(profile) {
        checkCompat();

        var self = this;
	var screen = false;

        self.type = "offer";

	if (self.options.videoParams && self.options.screenShare) { //self.options.videoParams.chromeMediaSource == 'desktop') {
	    screen = true;
	}

        function onSuccess(stream) {
	    self.localStream = stream;

	    if (screen) {
		self.constraints.offerToReceiveVideo = false;
		self.constraints.offerToReceiveAudio = false;
		self.constraints.offerToSendAudio = false;
	    }

            self.peer = FSRTCPeerConnection({
                type: self.type,
                attachStream: self.localStream,
                onICE: function(candidate) {
                    return onICE(self, candidate);
                },
                onICEComplete: function() {
                    return onICEComplete(self);
                },
                onRemoteStream: screen ? function(stream) {} : function(stream) {
                    return onRemoteStream(self, stream);
                },
                onOfferSDP: function(sdp) {
                    return onOfferSDP(self, sdp);
                },
                onICESDP: function(sdp) {
                    return onICESDP(self, sdp);
                },
                onChannelError: function(e) {
                    return onChannelError(self, e);
                },
                constraints: self.constraints,
                iceServers: self.options.iceServers,
                turnServer: self.options.turnServer
            });

            onStreamSuccess(self, stream);
        }

        function onError(e) {
            onStreamError(self, e);
        }

	var mediaParams = getMediaParams(self);

	console.log("Audio constraints", mediaParams.audio);
	console.log("Video constraints", mediaParams.video);

    if (self.options.useStream) {
      if (self.options.useVideo) {
        self.options.localVideoStream = self.options.useStream;
        if (self.options.localVideo) {
          activateLocalVideo(self.options.localVideo, self.options.useStream);
        }
      }
      onSuccess(self.options.useStream);
    }
    else if (mediaParams.audio || mediaParams.video) {
            getUserMedia({
		constraints: {
                    audio: mediaParams.audio,
                    video: mediaParams.video
		},
		video: mediaParams.useVideo,
		onsuccess: onSuccess,
        onerror: onError,
        useCameraLabel: self.options.useCameraLabel,
        useMicLabel: self.options.useMicLabel,
            });

	} else {
	    onSuccess(null);
	}



        /*
        navigator.getUserMedia({
            video: self.options.useVideo,
            audio: true
        }, onSuccess, onError);
        */

    };

    // DERIVED from RTCPeerConnection-v1.5
    // 2013, @muazkh - github.com/muaz-khan
    // MIT License - https://www.webrtc-experiment.com/licence/
    // Documentation - https://github.com/muaz-khan/WebRTC-Experiment/tree/master/RTCPeerConnection

    function FSRTCPeerConnection(options) {
	var gathering = false, done = false;
	var config = {};
        var default_ice = [{ urls: ['stun:stun.l.google.com:19302'] }];

        if (self.options.turnServer) {
          default_ice.push(self.options.turnServer)
        }

        if (options.iceServers) {
            if (typeof(options.iceServers) === "boolean") {
		config.iceServers = default_ice;
            } else {
		config.iceServers = options.iceServers;
	    }
        }

	config.bundlePolicy = "max-compat";
	config.sdpSemantics = "plan-b";

        var peer = new window.RTCPeerConnection(config);

        openOffererChannel();
        var x = 0;

	function ice_handler() {

	    done = true;
	    gathering = null;

            if (options.onICEComplete) {
                options.onICEComplete();
            }

            if (options.type == "offer") {
                options.onICESDP(peer.localDescription);
            } else {
                if (!x && options.onICESDP) {
                    options.onICESDP(peer.localDescription);
                }
            }
        }

        peer.onicecandidate = function(event) {

	    if (done) {
		return;
	    }

	    if (!gathering) {
		gathering = setTimeout(ice_handler, 1000);
	    }

	    if (event) {
		if (event.candidate) {
		    options.onICE(event.candidate);
		}
	    } else {
		done = true;

		if (gathering) {
		    clearTimeout(gathering);
		    gathering = null;
		}

		ice_handler();
	    }
        };

        // attachStream = MediaStream;
        if (options.attachStream) {
          // FreeSWITCH currently orders its answer SDP such that audio m-lines
          // always come first, adding the tracks to the peer in that order
          // prevents possible m-line ordering validation errors on the client.
          options.attachStream.getAudioTracks().forEach(function(track) { peer.addTrack(track, options.attachStream) });
          options.attachStream.getVideoTracks().forEach(function(track) { peer.addTrack(track, options.attachStream) });
        }

        // attachStreams[0] = audio-stream;
        // attachStreams[1] = video-stream;
        // attachStreams[2] = screen-capturing-stream;
        if (options.attachStreams && options.attachStreams.length) {
            var streams = options.attachStreams;
            for (var i = 0; i < streams.length; i++) {
                peer.addStream(streams[i]);
            }
        }

        // peer.onaddstream = function(event) { // OLD API
        peer.ontrack = function(event) {
            console.log('Peer Track', event)
            // var remoteMediaStream = event.stream;
            var remoteMediaStream = event.streams[0];

            remoteMediaStream.oninactive = function () {
                if (options.onRemoteStreamEnded) options.onRemoteStreamEnded(remoteMediaStream);
            };

            if (options.onRemoteStream) options.onRemoteStream(remoteMediaStream);
        };

        //var constraints = options.constraints || {
	  //  offerToReceiveAudio: true,
	    //offerToReceiveVideo: true
        //};

        // onOfferSDP(RTCSessionDescription)
        function createOffer() {
            if (!options.onOfferSDP) return;

            peer.createOffer(function(sessionDescription) {
                sessionDescription.sdp = serializeSdp(sessionDescription.sdp);
                peer.setLocalDescription(sessionDescription);
                options.onOfferSDP(sessionDescription);
            },
				onSdpError, options.constraints);
        }

        // onAnswerSDP(RTCSessionDescription)
        function createAnswer() {
            if (options.type != "answer") return;

            //options.offerSDP.sdp = addStereo(options.offerSDP.sdp);
            peer.setRemoteDescription(new window.RTCSessionDescription(options.offerSDP), onSdpSuccess, onSdpError);
            peer.createAnswer(function(sessionDescription) {
                sessionDescription.sdp = serializeSdp(sessionDescription.sdp);
                peer.setLocalDescription(sessionDescription);
                if (options.onAnswerSDP) {
                    options.onAnswerSDP(sessionDescription);
                }
            },
            onSdpError);
        }


        if ((options.onChannelMessage) || !options.onChannelMessage) {
            createOffer();
            createAnswer();
        }

        // DataChannel Bandwidth
        function setBandwidth(sdp) {
            // remove existing bandwidth lines
            sdp = sdp.replace(/b=AS([^\r\n]+\r\n)/g, '');
            sdp = sdp.replace(/a=mid:data\r\n/g, 'a=mid:data\r\nb=AS:1638400\r\n');

            return sdp;
        }

        // old: FF<>Chrome interoperability management
        function getInteropSDP(sdp) {
            var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split(''),
            extractedChars = '';

            function getChars() {
                extractedChars += chars[parseInt(Math.random() * 40)] || '';
                if (extractedChars.length < 40) getChars();

                return extractedChars;
            }

            // usually audio-only streaming failure occurs out of audio-specific crypto line
            // a=crypto:1 AES_CM_128_HMAC_SHA1_32 --------- kAttributeCryptoVoice
            if (options.onAnswerSDP) sdp = sdp.replace(/(a=crypto:0 AES_CM_128_HMAC_SHA1_32)(.*?)(\r\n)/g, '');

            // video-specific crypto line i.e. SHA1_80
            // a=crypto:1 AES_CM_128_HMAC_SHA1_80 --------- kAttributeCryptoVideo
            var inline = getChars() + '\r\n' + (extractedChars = '');
            sdp = sdp.indexOf('a=crypto') == -1 ? sdp.replace(/c=IN/g, 'a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:' + inline + 'c=IN') : sdp;

            return sdp;
        }

        function serializeSdp(sdp) {
            return sdp;
        }

        // DataChannel management
        var channel;

        function openOffererChannel() {
            if (!options.onChannelMessage) return;

            _openOffererChannel();

            return;
        }

        function _openOffererChannel() {
            channel = peer.createDataChannel(options.channel || 'RTCDataChannel', {
                reliable: false
            });

            setChannelEvents();
        }

        function setChannelEvents() {
            channel.onmessage = function(event) {
                if (options.onChannelMessage) options.onChannelMessage(event);
            };

            channel.onopen = function() {
                if (options.onChannelOpened) options.onChannelOpened(channel);
            };
            channel.onclose = function(event) {
                if (options.onChannelClosed) options.onChannelClosed(event);

                console.warn('WebRTC DataChannel closed', event);
            };
            channel.onerror = function(event) {
                if (options.onChannelError) options.onChannelError(event);

                console.error('WebRTC DataChannel error', event);
            };
        }

        function openAnswererChannel() {
            peer.ondatachannel = function(event) {
                channel = event.channel;
                channel.binaryType = 'blob';
                setChannelEvents();
            };

            return;
        }

        // fake:true is also available on chrome under a flag!
        function useless() {
            log('Error in fake:true');
        }

        function onSdpSuccess() {}

        function onSdpError(e) {
            if (options.onChannelError) {
                options.onChannelError(e);
            }
            console.error('sdp error:', e);
        }

        return {
            addAnswerSDP: function(sdp, cbSuccess, cbError) {

                peer.setRemoteDescription(new window.RTCSessionDescription(sdp), cbSuccess ? cbSuccess : onSdpSuccess, cbError ? cbError : onSdpError);
            },
            addICE: function(candidate) {
                peer.addIceCandidate(new window.RTCIceCandidate({
                    sdpMLineIndex: candidate.sdpMLineIndex,
                    candidate: candidate.candidate
                }));
            },

            peer: peer,
            channel: channel,
            sendData: function(message) {
                if (channel) {
                    channel.send(message);
                }
            },

            stop: function() {
                if (options.attachStream instanceof MediaStream) {
                    var tracks = options.attachStream.getTracks();
                    tracks.forEach(function(track){ track.stop() })
                    options.attachStream = null
                }
                peer.close();
            }
        };
    }

    // getUserMedia
    var video_constraints = {
        //mandatory: {},
        //optional: []
    };

  function activateLocalVideo(el, stream) {
    el.srcObject = stream;
    el.style.display = 'block';
  }

  function deactivateLocalVideo(el) {
    el.srcObject = null;
    el.style.display = 'none';
  }

    function assureConstraintByLabel(constraint, fallbackLabel) {
        if (fallbackLabel === undefined && constraint === undefined) {
            return Promise.resolve(constraint);
        }

        if (typeof(assureMediaInputId) !== 'function') {
            console.warn('Tried to use constraint fallbacks but did not found vendor function `assureMediaInputId` on window scope. Did you forget to import `vendor/media-device-id.js` before Verto?');
            return Promise.resolve(constraint);
        }

        if (typeof(constraint) === 'object' && !constraint.deviceId) {
            return Promise.resolve(constraint);
        }

        if (constraint.deviceId) {
            if (typeof(constraint.deviceId) === 'string') {
                return new Promise(function(resolve) {
                    assureMediaInputId(fallbackLabel, constraint.deviceId).then(function(id) {
                        resolve(Object.assign({}, constraint, { deviceId: id }));
                    }).catch(function() {
                        resolve(constraint);
                    });
                });
            }

            if (typeof(constraint.deviceId) === 'object' && typeof(constraint.deviceId.exact) === 'string') {
                return new Promise(function(resolve) {
                    assureMediaInputId(fallbackLabel, constraint.deviceId.exact).then(function(id) {
                        resolve(assignMediaIdToConstraint(id, constraint));
                    }).catch(function() {
                        resolve(constraint);
                    });
                });
            }
        }

        return Promise.resolve(constraint);
    }

    function trustyGetUserMedia(options, constraints) {
        navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
            if (options.localVideo) {
                activateLocalVideo(options.localVideo, stream);
            }

            if (options.onsuccess) {
                options.onsuccess(stream);
            }
        }).catch(options.onerror || function(e) {
            console.error(e);
        });
    }

    function assignMediaIdToConstraint(mediaId, rest) {
        return Object.assign({}, rest || {}, { deviceId: { exact: mediaId } });
    }

    function getUserMedia(options) {
        var constraints = options.constraints || {
            audio: true,
            video: video_constraints,
        };

        Promise.all([
            assureConstraintByLabel(constraints.audio, options.useMicLabel),
            assureConstraintByLabel(constraints.video, options.useCameraLabel),
        ]).then(function(assurances) {
            trustyGetUserMedia(options, { audio: assurances[0], video: assurances[1] });
        }).catch(function(error) {
            console.error('Unexpected error on media id assurance attempts:', error, 'Options:', options);
        });
    }
    $.FSRTC.resSupported = function(w, h) {
	for (var i in $.FSRTC.validRes) {
	    if ($.FSRTC.validRes[i][0] == w && $.FSRTC.validRes[i][1] == h) {
		return true;
	    }
	}

	return false;
    }

    $.FSRTC.bestResSupported = function() {
	var w = 0, h = 0;

	for (var i in $.FSRTC.validRes) {
	    if ($.FSRTC.validRes[i][0] >= w && $.FSRTC.validRes[i][1] >= h) {
		w = $.FSRTC.validRes[i][0];
		h = $.FSRTC.validRes[i][1];
	    }
	}

	return [w, h];
    }

    var resList = [[160, 120], [320, 180], [320, 240], [640, 360], [640, 480], [1280, 720], [1920, 1080]];
    var resI = 0;
    var ttl = 0;

    var checkRes = function (cam, func) {

	if (resI >= resList.length) {
            var res = {
               'validRes': $.FSRTC.validRes,
               'bestResSupported': $.FSRTC.bestResSupported()
            };

	    localStorage.setItem("res_" + cam, $.toJSON(res));

	    if (func) return func(res);
	    return;
	}

	w = resList[resI][0];
	h = resList[resI][1];
	resI++;

	var video = {
	    width: {exact: w},
	    height: {exact: h}
	};

  if (cam !== "any") {
    video = assignMediaIdToConstraint(cam, video);
  }

	    getUserMedia({
	    constraints: { audio: ttl++ == 0, video: video },
	    onsuccess: function(e) {
            e.getTracks().forEach(function(track) { track.stop() });
            console.info(w + "x" + h + " supported."); $.FSRTC.validRes.push([w, h]); checkRes(cam, func);
        },
	    onerror: function(e) {console.warn( w + "x" + h + " not supported."); checkRes(cam, func);}
        });
    }


    $.FSRTC.getValidRes = function (cam, func) {
	var used = [];
	var cached = localStorage.getItem("res_" + cam);

	if (cached) {
	    var cache = $.parseJSON(cached);

	    if (cache) {
		$.FSRTC.validRes = cache.validRes;
		console.log("CACHED RES FOR CAM " + cam, cache);
	    } else {
		console.error("INVALID CACHE");
	    }
	    return func ? func(cache) : null;
	}


	$.FSRTC.validRes = [];
	resI = 0;

	checkRes(cam, func);
    }

    $.FSRTC.checkPerms = function (runtime, check_audio, check_video) {
        getUserMedia({
            constraints: { audio: check_audio, video: check_video },
            onsuccess: function(e) {
                e.getTracks().forEach(function(track) { track.stop() });
                console.info("media perm init complete");
                if (runtime) {
                    setTimeout(runtime, 100, true);
                }
            },
            onerror: function(e) {
                if (check_video && check_audio) {
                    console.error("error, retesting with audio params only");
                    return $.FSRTC.checkPerms(runtime, check_audio, false);
                }
                console.error("media perm init error");
                if (runtime) {
                    runtime(false)
                }
            }
        });
    }

})(jQuery);
