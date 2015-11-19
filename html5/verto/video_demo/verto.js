'use strict';
var cur_call = null;
var share_call = null;
var confMan = null;
var vertoHandle;
var ringing = false;
var autocall = false;
var chatting_with = false;

var vid_width = 320;
var vid_height = 180;

var local_vid_width = 320;
var local_vid_height = 180;
var is_full_screen = false;
var outgoingBandwidth;
var incomingBandwidth;
var vqual;
var sessid = null;
var master = null;
var canvas_id = null;
var second_screen = null;
var save_settings = true;

var video_screen = "webcam"

$( ".selector" ).pagecontainer({ "theme": "a" });

function display(msg) {
    $("#calltitle").html(msg);
}

function clearConfMan() {
    if (confMan) {
        confMan.destroy();
        confMan = null;
    }

    $("#conf").hide();
    $("#canvasui").hide();
    $("#message").hide();
    chatting_with = null;
}

function goto_dialog(where) {
    $( ":mobile-pagecontainer" ).pagecontainer( "change", "#dialog-" + where, { role: "dialog" } );
}

function goto_page(where, force) {
    $( ":mobile-pagecontainer" ).pagecontainer( "change", "#page-" + where);
}

var first_login = false;
var online_visible = false;
function online(on) {
    if (on) {
        $("#online").show();
        $("#offline").hide();
        first_login = true;
    } else {

        $("#online").hide();
        $("#offline").show();
    }

    online_visible = on;
}

function setupChat() {
    $("#chatwin").html("");

    $("#chatsend").click(function() {
	if (!cur_call || !chatting_with || !confMan) {
	    return;
	}

	confMan.sendChat($("#chatmsg").val(), "message");
	$("#chatmsg").val("");
    });

    $("#chatmsg").keyup(function (event) {
	if (event.keyCode == 13 && !event.shiftKey) {
	    $( "#chatsend" ).trigger( "click" );   
	}
    });

}

function full_screen(name) {
    var elem = document.getElementById(name);
    if (!elem) return;
    if (elem.requestFullscreen) {
	elem.requestFullscreen();
    } else if (elem.msRequestFullscreen) {
	elem.msRequestFullscreen();
    } else if (elem.mozRequestFullScreen) {
	elem.mozRequestFullScreen();
    } else if (elem.webkitRequestFullscreen) {
	elem.webkitRequestFullscreen();
    }
}

$("#" + video_screen).resize(function(e) { 
    console.log("video size changed to " + $("#" + video_screen).width() + "x" + $("#" + video_screen).height());

    if ($("#" + video_screen).width() > $(window).width()) {
	//resize(false);
	$("#" + video_screen).width("100%");
	$("#" + video_screen).height("100%"); 
    }

});
		   

function resize(up) {
    var width = $("#" + video_screen).width();
    var height = $("#" + video_screen).height();

    if (up) {
	$("#" + video_screen).width(width * 1.20);
	$("#" + video_screen).height(height * 1.20);
    } else {
	$("#" + video_screen).width(width * .80);
	$("#" + video_screen).height(height * .80);
    }

    console.log("video size changed to " + $("#" + video_screen).width() + "x" + $("#" + video_screen).height());

}

function real_size() {


    $("#" + video_screen).width("");
    $("#" + video_screen).height("");

    console.log("video size changed to natural default");

}

function check_vid_res()
{
    if ($("#vqual_qvga").is(':checked')) {
	vid_width = 320;
	vid_height = 240;
	local_vid_width = 160;
	local_vid_height = 120;
    } else if ($("#vqual_vga").is(':checked')) {
	vid_width = 640;
	vid_height = 480;
	local_vid_width = 160;
	local_vid_height = 120;
    } else if ($("#vqual_qvga_wide").is(':checked')) {
	vid_width = 320;
	vid_height = 180;
	local_vid_width = 160;
	local_vid_height = 90;
    } else if ($("#vqual_vga_wide").is(':checked')) {
	vid_width = 640;
	vid_height = 360;
	local_vid_width = 160;
	local_vid_height = 90;
    } else if ($("#vqual_hd").is(':checked')) {
	vid_width = 1280;
	vid_height = 720;
	local_vid_width = 320;
	local_vid_height = 180;
    } else if ($("#vqual_hhd").is(':checked')) {
	vid_width = 1920;
	vid_height = 1080;
	local_vid_width = 320;
	local_vid_height = 180;
    }

    //$("#local_webcam").width(local_vid_width);
    //$("#local_webcam").height(local_vid_height);

    real_size();

    if (vertoHandle) {
	vertoHandle.videoParams({
	    "minWidth": vid_width,
	    "minHeight": vid_height,
	    "maxWidth": vid_width,
	    "maxHeight": vid_height,
	    "minFrameRate": 15, 
	    "vertoBestFrameRate": 30,
	    //chromeMediaSource: 'screen', 
	    //mediaSource: 'screen'
	});
    }

}

function check_vid() {
    var use_vid = $("#use_vid").is(':checked');
    return use_vid;
}

function do_speed_test(fn)
{
    goto_page("bwtest");

    vertoHandle.rpcClient.speedTest(1024 * 256, function(e, obj) {
	//console.error("Up: " + obj.upKPS, "Down: ", obj.downKPS);
	var vid = "default";
	//if (outgoingBandwidth === "default") {
	    outgoingBandwidth = Math.ceil(obj.upKPS * .75).toString();
	    
	    $("#vqual_hd").prop("checked", true);
	    vid = "1280x720";

	    if (outgoingBandwidth < 1024) {
		$("#vqual_vga").prop("checked", true);
		vid = "640x480";
	    }
	    if (outgoingBandwidth < 512) {
		$("#vqual_qvga").prop("checked", true);
		vid = "320x240";
	    }
	//}

	if (incomingBandwidth === "default") {
	    incomingBandwidth = Math.ceil(obj.downKPS * .75).toString();
	}

	console.info(outgoingBandwidth, incomingBandwidth);

	$("#bwinfo").html("<b>Bandwidth: " + "Up: " + obj.upKPS + " Down: " + obj.downKPS + " Vid: " + vid + "</b>");

	if (fn) {
	    fn();
	}
    });
}

function messageTextToJQ(body) {
	// Builds a jQuery collection from body text, linkifies http/https links, imageifies http/https links to images, and doesn't allow script injection
	
	var match, $link, img_url, $body_parts = $(), rx = /(https?:\/\/[^ \n\r]+|\n\r|\n|\r)/;
	
	while ((match = rx.exec(body)) !== null) {
		if (match.index !== 0) {
			$body_parts = $body_parts.add(document.createTextNode(body.substr(0, match.index)));
		}

		if (match[0].match(/^(\n|\r|\n\r)$/)) {
			// Make a BR from a newline
			$body_parts = $body_parts.add($('<br />'));
			body = body.substr(match.index + match[0].length);
		} else {
			// Make a link (or image)
			$link = $('<a target="_blank" />').attr('href', match[0]);
			
			if (match[0].search(/\.(gif|jpe?g|png)/) > -1) {
				// Make an image
				img_url = match[0];

				// Handle dropbox links
				if (img_url.indexOf('dropbox.com') !== -1) {
					if (img_url.indexOf('?dl=1') === -1 && img_url.indexOf('?dl=0') === -1) {
						img_url += '?dl=1';
					} else if (img_url.indexOf('?dl=0') !== -1) {
						img_url = img_url.replace(/dl=0$/, 'dl=1');
					}
				}

				$link.append($('<img border="0" class="chatimg" />').attr('src', img_url));
			} else {
				// Make a link
				$link.text(match[0]);
			}

			body = body.substr(match.index + match[0].length);
			$body_parts = $body_parts.add($link);
		}
	}
	if (body) {
		$body_parts = $body_parts.add(document.createTextNode(body));
	}

	return $body_parts;
} // END function messageTextToJQ

var callbacks = {

    onMessage: function(verto, dialog, msg, data) {

        switch (msg) {
        case $.verto.enum.message.pvtEvent:
//            console.error("pvtEvent", data.pvtData);
            if (data.pvtData) {
                switch (data.pvtData.action) {

                case "conference-liveArray-part":
                    clearConfMan();
		    if (data.pvtData.secondScreen) {
			$("#mainButtons").show();
			$("#canvasButtons").hide();
			$("#keypad").show();
		    }
                    break;
                case "conference-liveArray-join":
                    clearConfMan();

		    if (data.pvtData.secondScreen) {
			$("#mainButtons").hide();
			$("#canvasButtons").show();
			$("#keypad").hide();
		    } else {
			confMan = new $.verto.confMan(verto, {
			    tableID: "#conf_list",
			    statusID: "#conf_count",
			    mainModID: "#conf_mod",
			    displayID: "#conf_display",
			    dialog: dialog,
			    hasVid: check_vid(),
			    laData: data.pvtData,
			    chatCallback: function(v, e) {
				console.log(e);
				var from = e.data.fromDisplay || e.data.from || "Unknown";
				var message = e.data.message || "";

				$('#chatwin')
				    .append($('<span class="chatuid" />').text(from + ':'))
				    .append($('<br />'))
				    .append(messageTextToJQ(message))
				    .append($('<br />'));
				$('#chatwin').animate({"scrollTop": $('#chatwin')[0].scrollHeight}, "fast");
			    }
			});

			if (!data.pvtData.canvasCount) {
			    data.pvtData.canvasCount = 1;
			}

			var canvasCount = data.pvtData.canvasCount + 0;
		    
			if (canvasCount <= 1) {
			    $("#canvasui").hide();
			} if (canvasCount > 1) {
			    $("#canvasui").show();
			    $("#canvasid").selectmenu({});
			    $("#canvasid").selectmenu("enable");
			    $("#canvasid").empty();
			    
			    var x;
			    
			    for (x = 1; x < canvasCount; x++) {
				$("#canvasid").append(new Option("Canvas " + (x + 1), (x + 1)));
			    }
			    
			    $("#canvasid").append(new Option("Super Canvas", x + 1));

			    $("#canvasid").selectmenu('refresh', true);
			    
			    $("#canvasbut").click(function() {
				var canvas_id = $("#canvasid").find(":selected").val();
				var s = window.location.href;
				s = s.replace(/\#.*/,'');
				s += "#sessid=random&master=" + cur_call.callID + 
				    "&secondScreen=true&canvas_id=" + canvas_id + "&autocall=" + $("#ext").val() + "-canvas-" + canvas_id;
				console.log("opening new window to " + s);
				window.open(s, "canvas_window_" + canvas_id, "toolbar=0,location=0,menubar=0,directories=0,width=" + ($("#" + video_screen).width() + 50) + ",height=" + ($("#" + video_screen).height() + 400));
			    });
			}

			$("#conf").show();
			$("#chatwin").html("");

			if (data.pvtData.hipchatURL) {
			    var namex = $("#cidname").val();

			    if (!namex.indexOf(" ") > 0) {
				namex += " " + $("#cid").val();
			    }
			    
			    var name = namex.replace(/ /i, '%20');
			
			    $('#hcmessage').hipChatPanel({
				url: data.pvtData.hipchatURL + "?name=" + name,
				timezone: "CST"
			    });
			    $("#hctop").show().find('.show-hipchat').click();
			} else {
			    $("#message").show();
			}

			chatting_with = data.pvtData.chatChannel;
		    }

                    break;
                }
            }
            break;
        case $.verto.enum.message.info:
	    var body = data.body;

		/*
		// This section has been replaced with messageTextToJQ function

	    if (body.match(/\.gif|\.jpg|\.jpeg|\.png/)) {
		var mod = "";
		if (body.match(/dropbox.com/)) {
		    mod = "?dl=1";
		}
		body = body.replace(/(http[s]{0,1}:\/\/\S+)/g, "<a target='_blank' href='$1'>$1<br><img border='0' class='chatimg' src='$1'" + mod + "><\/a>");
	    } else {
		body = body.replace(/(http[s]{0,1}:\/\/\S+)/g, "<a target='_blank' href='$1'>$1<\/a>");
	    }

	    if (body.slice(-1) !== "\n") {
		body += "\n";
	    }
	    body = body.replace(/(?:\r\n|\r|\n)/g, '<br />');
	    
    	    var from = data.from_msg_name || data.from;

            $("#chatwin").append("<span class=chatuid>" + from + ":</span><br>" + body);
	    $('#chatwin').animate({"scrollTop": $('#chatwin')[0].scrollHeight}, "fast");
		*/
		
			var from = data.from_msg_name || data.from;
			
			$('#chatwin')
				.append($('<span class="chatuid" />').text(from + ':'))
				.append($('<br />'))
				.append(messageTextToJQ(body))
				.append($('<br />'));
			$('#chatwin').animate({"scrollTop": $('#chatwin')[0].scrollHeight}, "fast");
		
            break;
        case $.verto.enum.message.display:
            var party = dialog.params.remote_caller_id_name + "<" + dialog.params.remote_caller_id_number + ">";
            display("Talking to: " + dialog.cidString());
            break;
        default:
            break;
        }
    },

    onDialogState: function(d) {

	//console.error(d, share_call, d == share_call, d.state);

	if (d == share_call) {
            switch (d.state) {
            case $.verto.enum.state.early:
            case $.verto.enum.state.active:
		$("#nosharebtn").show();
		$("#sharebtn").hide();
		break;
            case $.verto.enum.state.destroy:
		$("#nosharebtn").hide();
		$("#sharebtn").show();
		share_call = null;
		break;
	    }

	    return;
	}

	if (!cur_call) {
            cur_call = d;
	}
	
	if (d.state == $.verto.enum.state.ringing) {
	    ringing = true;
	} else {
	    ringing = false;
	}

        switch (d.state) {
        case $.verto.enum.state.ringing:
            display("Call From: " + d.cidString());

	    check_vid_res();

            $("#ansbtn").click(function() {
                cur_call.answer({
		    useStereo: $("#use_stereo").is(':checked'),
		    callee_id_name: $("#cidname").val(),
		    callee_id_number: $("#cid").val(),
		    useCamera: $("#usecamera").find(":selected").val(),
		    useMic: $("#usemic").find(":selected").val(),
		    useSpeak: $("#usespeak").find(":selected").val()

		});
                $('#dialog-incoming-call').dialog('close');
            });

            $("#declinebtn").click(function() {
                cur_call.hangup();
                $('#dialog-incoming-call').dialog('close');
            });

            goto_dialog("incoming-call");
            $("#dialog-incoming-call-txt").text("Incoming call from: " + d.cidString());

            if (d.params.wantVideo) {
                $("#vansbtn").click(function() {
                    $("#use_vid").prop("checked", true);
                    cur_call.answer({
                        useVideo: true,
			useStereo: $("#use_stereo").is(':checked'),
			useCamera: $("#usecamera").find(":selected").val(),
			useMic: $("#usemic").find(":selected").val(),
			useSpeak: $("#usespeak").find(":selected").val()
                    });
                });
                // the buttons in this jquery mobile wont hide .. gotta wrap them in a div as a workaround
                $("#vansdiv").show();
            } else {
                $("#vansdiv").hide();
            }

            break;

        case $.verto.enum.state.trying:
            display("Calling: " + d.cidString());
            goto_page("incall");
	    break;
        case $.verto.enum.state.early:
        case $.verto.enum.state.active:
	    if (sessid) {
		cur_call.setMute("on");
		display("Viewing Canvas: " + canvas_id);

		vertoHandle.subscribe("presence", {
                    handler: function(v, e) {
			if (e.data.channelUUID === master && e.data.channelCallState === "HANGUP") {
			    cur_call.hangup();
			}
                    }
		});

	    } else {
		display("Talking to: " + d.cidString());
	    }

            goto_page("incall");

            break;
        case $.verto.enum.state.hangup:
	    $("#main_info").html("Call ended with cause: " + d.cause);
            goto_page("main");
	    exit_full_screen();
        case $.verto.enum.state.destroy:
	    $("#hangup_cause").html("");
            clearConfMan();
	    real_size();
            cur_call = null;
	    if (sessid) {
		setTimeout(function() {
		    delete $.verto.warnOnUnload;
		    window.close();
		}, 500);
	    }
            break;
        case $.verto.enum.state.held:
            break;
        default:
            display("");
            break;
        }
    },
    onWSLogin: function(v, success) {
        display("");

	cur_call = null;
	ringing = false;

        if (success) {

	    do_speed_test(function() {
		
		online(true);
		goto_page("main");

		$("input[type='radio']").checkboxradio("refresh");
		$("input[type='checkbox']").checkboxradio("refresh");


		/*
		  verto.subscribe("presence", {
                  handler: function(v, e) {
                  console.error("PRESENCE:", e);
                  }
		  });
		*/
		
		if (!window.location.hash) {
                    goto_page("main");
		}
		
		if (autocall) {
		    autocall = false;
		    docall();
		}
	    });

        } else {
            goto_page("main");
            goto_dialog("login-error");
        }

    },
    onWSClose: function(v, success) {
        display("");
        online(false);
        var today = new Date();
        $("#errordisplay").html("Connection Error.<br>Last Attempt: " + today);
        goto_page("main");

	if (sessid) {
	    window.close();
	}
    },

    onEvent: function(v, e) {
        console.debug("GOT EVENT", e);
    },
};

$("#hold").click(function(e) {
    cur_call.toggleHold();
    goto_dialog("hold");
});

$("#cancelxferbtn").click(function(e) {
    $("#xferto").val("");
    $("#xferdiv").hide();
});

$(".startxferbtn").click(function(e) {
    if ($('#xferdiv').is(':visible')) {
        var xfer = $("#xferto").val();
        if (xfer) {
            cur_call.transfer(xfer);
        }
        $("#xferto").val("");
        $("#xferdiv").hide();
    } else {
        $("#xferdiv").show();
    }
});

$("#clearbtn").click(function(e) {
    $("#ext").val("");
});

$(".dialbtn").click(function(e) {
    $("#ext").val($("#ext").val() + e.currentTarget.textContent);
});

$(".dtmf").click(function(e) {
    if ($('#xferdiv').is(':visible')) {
        $("#xferto").val($("#xferto").val() + e.currentTarget.textContent);
    } else {
        cur_call.dtmf(e.currentTarget.textContent);
    }

});

$("#hupbtn").click(function() {
    exit_full_screen();
    vertoHandle.hangup();
    cur_call = null;
});

$("#hupbtn2").click(function() {
    delete $.verto.warnOnUnload;
    vertoHandle.hangup();
    cur_call = null;
});

$("#mutebtn").click(function() {
    cur_call.dtmf("0");
});

$("#localmutebtn").click(function() {
    var muted = cur_call.setMute("toggle");

    if (muted) {
	display("Talking to: " + cur_call.cidString() + " [LOCALLY MUTED]");
    } else {
	display("Talking to: " + cur_call.cidString());
    }

});

$("#localvidmutebtn").click(function() {
    var muted = cur_call.setVideoMute("toggle");

    if (muted) {
	display("Talking to: " + cur_call.cidString() + " [VIDEO LOCALLY MUTED]");
    } else {
	display("Talking to: " + cur_call.cidString());
    }

});

$("#vmutebtn").click(function() {
    cur_call.dtmf("*0");
});

var is_full = false;
var usrto;
var rs;
function noop() { return; }

function on_full(which)
{
    is_full = which;
    if (is_full) {
	clearTimeout(rs);
	$("#usr2").hide();
	rs = setTimeout(function() {
	    $("#" + video_screen).width($(window).width());
	    $("#" + video_screen).height($(window).height());
	}, 1500);
	$("#rows").css("position", "absolute").css("z-index", "2");    
	$("#fullbtn").text("Exit Full Screen");
	$("#fullbtn2").text("Exit Full Screen");
	$("#usrctl").show();
    } else {
	$("#usrctl").hide();
	$("#rows").css("position", "static").css("z-index", "2");
	$("#fullbtn").text("Enter Full Screen");
	$("#fullbtn2").text("Enter Full Screen");
	clearTimeout(usrto);
	clearTimeout(rs);
	rs = setTimeout(function() { 
	    $("#" + video_screen).width("100%");
	    $("#" + video_screen).height("100%");
	}, 1500);
    }

}


$(document).on('webkitfullscreenchange mozfullscreenchange fullscreenchange MSFullscreenChange', 
	       function(e) {
		   if (!is_full) {
		       on_full(true);
		   } else {
		       on_full(false);
		   }
	       });


function exit_full_screen()
{
    if (document.webkitFullscreenEnabled) {
	document.webkitExitFullscreen();
    } else if (document.mozFullScreenEnabled) {
	document.mozCancelFullScreen();
    }
}

$("#fullbtn").click(function() {

    if (!is_full) {
	full_screen("fs");
    } else {
	exit_full_screen();
    }
});

$("#fullbtn2").click(function() {

    if (!is_full) {
	full_screen("fs");
    } else {
	exit_full_screen();
    }
});

$("#biggerbtn").click(function() {
    resize(true);
});

$("#smallerbtn").click(function() {
    resize(false);
});

$("#" + video_screen).click(function() {
    check_vid();
});

function docall() {
    $('#ext').trigger('change');

    if (cur_call) {
        return;
    }

    $("#main_info").html("Trying");

    check_vid_res();
    console.error(outgoingBandwidth, incomingBandwidth);
    cur_call = vertoHandle.newCall({
        destination_number: $("#ext").val(),
        caller_id_name: $("#cidname").val(),
        caller_id_number: $("#cid").val(),
	outgoingBandwidth: outgoingBandwidth,
	incomingBandwidth: incomingBandwidth,
        useVideo: check_vid(),
        useStereo: $("#use_stereo").is(':checked'),
	useCamera: sessid ? "none" : $("#usecamera").find(":selected").val(),
	useMic: $("#usemic").find(":selected").val(),
	useSpeak: $("#usespeak").find(":selected").val(),
	dedEnc: $("#use_dedenc").is(':checked'),
	mirrorInput: $("#mirror_input").is(':checked'),
        userVariables: {
            avatar: $("#avatar").val(),
            email: $("#email").val(),
        },
    });
}


function doshare(on) {
    //$('#ext').trigger('change');

    if (!on) {
	if (share_call) {
	    share_call.hangup();
	}

	return;
    }


    if (share_call) {
        return;
    }

    var sharedev = $("#useshare").find(":selected").val();

    if (sharedev !== "screen") {

	share_call = vertoHandle.newCall({
            destination_number: $("#ext").val() + "-screen",
            caller_id_name: $("#cidname").val() + " (Screen)",
            caller_id_number: $("#cid").val() + " (screen)",
	    outgoingBandwidth: outgoingBandwidth,
	    incomingBandwidth: incomingBandwidth,
	    useCamera: sharedev,
            useVideo: true,
	    screenShare: true,
	    dedEnc: $("#use_dedenc").is(':checked'),
	    mirrorInput: $("#mirror_input").is(':checked')
	});

	return;
    }


    console.log("Attempting Screen Capture....");
    getScreenId(function (error, sourceId, screen_constraints) {
	


	share_call = vertoHandle.newCall({
            destination_number: $("#ext").val() + "-screen",
            caller_id_name: $("#cidname").val() + " (Screen)",
            caller_id_number: $("#cid").val() + " (screen)",
	    outgoingBandwidth: outgoingBandwidth,
	    incomingBandwidth: incomingBandwidth,
	    videoParams: screen_constraints.video.mandatory,
            useVideo: true,
	    screenShare: true,
	    dedEnc: $("#use_dedenc").is(':checked'),
	    mirrorInput: $("#mirror_input").is(':checked')
	});

    });



    //$("#main_info").html("Trying");

    //check_vid_res();

    //cur_share = vertoHandle.newCall({
    //    destination_number: $("#ext").val(),
    //    caller_id_name: $("#cidname").val(),
    //    caller_id_number: $("#cid").val(),
    //    useVideo: check_vid(),
    //    useStereo: $("#use_stereo").is(':checked')
    //});
}

$("#callbtn").click(function() {
    docall();
});

$("#refreshbtn").click(function() {
    refresh_devices();
});

$("#sharebtn").click(function() {
    doshare(true);
});

$("#nosharebtn").click(function() {
    doshare(false);
});

$("#nosharebtn").hide();

function pop(id, cname, dft, onchange) {
    var tmp = $.cookie(cname) || dft;
    $.cookie(cname, tmp, {
        expires: 365
    });

    $(id).val(tmp).change(function() {
	if (!save_settings) return;

        $.cookie(cname, $(id).val(), {
            expires: 365
        });

	if (onchange) {
	    onchange($(id));
	}
    });
}

function pop_select(id, cname, dft, onchange) {
    var tmp = $.cookie(cname) || dft;
    $.cookie(cname, tmp, {
	expires: 365
    });
        // $("#usecamera").find(":selected").val()
    $(id).change(function() {
	if (!save_settings) return;

	tmp =  $(id).find(":selected").val();
	$.cookie(cname, tmp, {
	    expires: 365
	});

	if (onchange) {
	    onchange($(id));
	}
    });
}


function refresh_devices()
{

    $("#useshare").selectmenu({});
    $("#useshare").selectmenu({});
    $("#usemic").selectmenu({});
    $("#usespeak").selectmenu({});

    $("#useshare").selectmenu("enable");
    $("#useshare").selectmenu("enable");
    $("#usemic").selectmenu("enable");
    $("#usespeak").selectmenu("enable");

    $("#useshare").empty();
    $("#usecamera").empty();
    $("#usemic").empty();
    $("#usespeak").empty();
    


    var x = 0;

    $("#usecamera").append(new Option("No Camera", "none"));
    $("#usemic").append(new Option("Do Not Specify", "any"));
    $("#usespeak").append(new Option("Do Not Specify", "any"));
    for (var i in $.verto.videoDevices) {
	var source = $.verto.videoDevices[i];
	var o = new Option(source.label, source.id);
	if (!x) {
	    o.selected = true;
	}
	$("#usecamera").append(o);

	var oo = new Option(source.label, source.id);
	if (!x++) {
	    o.selected = true;
	}

	$("#useshare").append(oo);
    }

    x = 1;
    
    for (var i in $.verto.audioInDevices) {
	var source = $.verto.audioInDevices[i];
	var o = new Option(source.label, source.id);
	if (!x++) {
	    o.selected = true;
	}
	$("#usemic").append(o);
    }

    for (var i in $.verto.audioOutDevices) {
	var source = $.verto.audioOutDevices[i];
	var o = new Option(source.label, source.id);
	if (!x++) {
	    o.selected = true;
	}
	$("#usespeak").append(o);
    }


    var o = new Option("Screen", "screen");
    o.selected = true;

    $("#useshare").append(o);

    $("#usemic").append(new Option("No Microphone", "none"));

    
    $("#usecamera").selectmenu('refresh', true);
    $("#usemic").selectmenu('refresh', true);
    $("#usespeak").selectmenu('refresh', true);
    $("#useshare").selectmenu('refresh', true);

    //$("input[type='radio']).checkboxradio({});


    //$("input[type='radio']").checkboxradio("refresh");
    //$("input[type='checkbox']").checkboxradio("refresh");

    //console.error($("#usecamera").find(":selected").val());
    //$.FSRTC.getValidRes($("#usecamera").find(":selected").val(), undefined);

    var tmp;
    tmp = $.cookie("verto_demo_camera_selected") || "false";
    if (tmp) {
        $('#usecamera option[value=' + tmp + ']').prop('selected', 'selected').change();
        pop_select("#usecamera","verto_demo_camera_selected", tmp);
    }

    tmp = $.cookie("verto_demo_mic_selected") || "false";
    if (tmp) {
        $('#usemic option[value=' + tmp + ']').prop('selected', 'selected').change();
        pop_select("#usemic","verto_demo_mic_selected", tmp);
    }

    tmp = $.cookie("verto_demo_speak_selected") || "false";
    if (tmp) {
        $('#usespeak option[value=' + tmp + ']').prop('selected', 'selected').change();
        pop_select("#usespeak","verto_demo_speak_selected", tmp);
    }
}

function init() {
    cur_call = null;
    goto_page("bwtest");

    $("#usecamera").selectmenu({});
    $("#usemic").selectmenu({});
    $("#usespeak").selectmenu({});
    $("#useshare").selectmenu({});

    if (!autocall) {
	pop("#ext", "verto_demo_ext", "3500");
    }

    pop("#avatar", "verto_demo_avatar", "", function(jq) { 
	$("#avatar_img").attr("src", jq.val());
    });
    pop("#cidname", "verto_demo_name", "FreeSWITCH User");
    pop("#cid", "verto_demo_cid", "1008");
    pop("#email", "verto_demo_emailaddr", "", function(jq) {
	$("#avatar").val("http://gravatar.com/avatar/" + md5($("#email").val()) + ".png?s=600");
	$("#avatar_img").attr("src", $("#avatar").val());
	$("#avatar").change();
    });
    pop("#textto", "verto_demo_textto", "1000");

    pop("#login", "verto_demo_login", "1008");
    pop("#passwd", "verto_demo_passwd", "1234");

    pop("#hostName", "verto_demo_hostname", window.location.hostname);
    pop("#wsURL", "verto_demo_wsurl", "wss://" + window.location.hostname + ":8082");


    $("#avatar_img").attr("src", $("#avatar").val());

    var tmp = $.cookie("verto_demo_vid_checked") || "true";
    $.cookie("verto_demo_vid_checked", tmp, {
        expires: 365
    });

    if (tmp !== "true") {
	$("#camdiv").hide();
	$(".sharediv").hide();
    } else {
	$(".sharediv").show();
	$("#camdiv").show();
    }

    $("#use_vid").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_vid").is(':checked');

	if (!tmp) {
	    $("#camdiv").hide();
	    $(".sharediv").hide();
	} else {
	    $("#camdiv").show();
	    $(".sharediv").show();
	}
        $.cookie("verto_demo_vid_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });


    tmp = $.cookie("verto_demo_dedenc_checked") || "false";
    $.cookie("verto_demo_dedenc_checked", tmp, {
        expires: 365
    });

    $("#use_dedenc").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_dedenc").is(':checked');

	if (!tmp && $("#mirror_input").is(':checked')) {
	    $("#mirror_input").click();
	}
	
        $.cookie("verto_demo_dedenc_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });


    tmp = $.cookie("verto_demo_mirror_input_checked") || "false";
    $.cookie("verto_demo_mirror_input_checked", tmp, {
        expires: 365
    });

    $("#mirror_input").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#mirror_input").is(':checked');
	if (tmp && !$("#use_dedenc").is(':checked')) {
	    $("#use_dedenc").click();
	}
        $.cookie("verto_demo_mirror_input_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });

//
    outgoingBandwidth = $.cookie("verto_demo_outgoingBandwidth") || "default";
    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
        expires: 365
    });

    $("#outgoingBandwidth_250kb").prop("checked", outgoingBandwidth === "250").change(function(e) {
        if ($("#outgoingBandwidth_250kb").is(':checked')) {
	    outgoingBandwidth = "250";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_500kb").prop("checked", outgoingBandwidth === "500").change(function(e) {
        if ($("#outgoingBandwidth_500kb").is(':checked')) {
	    outgoingBandwidth = "500";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_1024kb").prop("checked", outgoingBandwidth === "1024").change(function(e) {
        if ($("#outgoingBandwidth_1024kb").is(':checked')) {
	    outgoingBandwidth = "1024";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_1536kb").prop("checked", outgoingBandwidth === "1536").change(function(e) {
        if ($("#outgoingBandwidth_1536kb").is(':checked')) {
	    outgoingBandwidth = "1536";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_2048kb").prop("checked", outgoingBandwidth === "2048").change(function(e) {
        if ($("#outgoingBandwidth_2048kb").is(':checked')) {
	    outgoingBandwidth = "2048";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_5120kb").prop("checked", outgoingBandwidth === "5120").change(function(e) {
        if ($("#outgoingBandwidth_5120kb").is(':checked')) {
	    outgoingBandwidth = "5120";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_0kb").prop("checked", outgoingBandwidth === "0").change(function(e) {
        if ($("#outgoingBandwidth_0kb").is(':checked')) {
	    outgoingBandwidth = "0";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#outgoingBandwidth_default").prop("checked", outgoingBandwidth === "default").change(function(e) {
        if ($("#outgoingBandwidth_default").is(':checked')) {
	    outgoingBandwidth = "default";
	    $.cookie("verto_demo_outgoingBandwidth", outgoingBandwidth, {
		expires: 365
	    });
	}
    });
//

    incomingBandwidth = $.cookie("verto_demo_incomingBandwidth") || "default";
    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
        expires: 365
    });

    $("#incomingBandwidth_250kb").prop("checked", incomingBandwidth === "250").change(function(e) {
        if ($("#incomingBandwidth_250kb").is(':checked')) {
	    incomingBandwidth = "250";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_500kb").prop("checked", incomingBandwidth === "500").change(function(e) {
        if ($("#incomingBandwidth_500kb").is(':checked')) {
	    incomingBandwidth = "500";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_1024kb").prop("checked", incomingBandwidth === "1024").change(function(e) {
        if ($("#incomingBandwidth_1024kb").is(':checked')) {
	    incomingBandwidth = "1024";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_1536kb").prop("checked", incomingBandwidth === "1536").change(function(e) {
        if ($("#incomingBandwidth_1536kb").is(':checked')) {
	    incomingBandwidth = "1536";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_2048kb").prop("checked", incomingBandwidth === "2048").change(function(e) {
        if ($("#incomingBandwidth_2048kb").is(':checked')) {
	    incomingBandwidth = "2048";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_5120kb").prop("checked", incomingBandwidth === "5120").change(function(e) {
        if ($("#incomingBandwidth_5120kb").is(':checked')) {
	    incomingBandwidth = "5120";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_0kb").prop("checked", incomingBandwidth === "0").change(function(e) {
        if ($("#incomingBandwidth_0kb").is(':checked')) {
	    incomingBandwidth = "0";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });

    $("#incomingBandwidth_default").prop("checked", incomingBandwidth === "default").change(function(e) {
        if ($("#incomingBandwidth_default").is(':checked')) {
	    incomingBandwidth = "default";
	    $.cookie("verto_demo_incomingBandwidth", incomingBandwidth, {
		expires: 365
	    });
	}
    });
//

    vqual = $.cookie("verto_demo_vqual") || "hd";
    $.cookie("verto_demo_vqual", vqual, {
        expires: 365
    });



    $("#vqual_qvga").prop("checked", vqual === "qvga").change(function(e) {
        if ($("#vqual_qvga").is(':checked')) {
	    vqual = "qvga";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });


    $("#vqual_vga").prop("checked", vqual === "vga").change(function(e) {
        if ($("#vqual_vga").is(':checked')) {
	    vqual = "vga";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });


    $("#vqual_qvga_wide").prop("checked", vqual === "qvga_wide").change(function(e) {
        if ($("#vqual_qvga_wide").is(':checked')) {
	    vqual = "qvga_wide";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });


    $("#vqual_vga_wide").prop("checked", vqual === "vga_wide").change(function(e) {
        if ($("#vqual_vga_wide").is(':checked')) {
	    vqual = "vga_wide";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });


    $("#vqual_hd").prop("checked", vqual === "hd").change(function(e) {
        if ($("#vqual_hd").is(':checked')) {
	    vqual = "hd";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });


    $("#vqual_hhd").prop("checked", vqual === "hhd").change(function(e) {
        if ($("#vqual_hhd").is(':checked')) {
	    vqual = "hhd";
	    $.cookie("verto_demo_vqual", vqual, {
		expires: 365
	    });
	}
    });
    
    
    tmp = $.cookie("verto_demo_stereo_checked") || "true";
    $.cookie("verto_demo_stereo_checked", tmp, {
        expires: 365
    });

    $("#use_stereo").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_stereo").is(':checked');
        $.cookie("verto_demo_stereo_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });

    tmp = $.cookie("verto_demo_stun_checked") || "true";
    $.cookie("verto_demo_stun_checked", tmp, {
        expires: 365
    });

    $("#use_stun").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_stun").is(':checked');
        $.cookie("verto_demo_stun_checked", tmp ? "true" : "false", {
            expires: 365
        });
	if (vertoHandle) {
	    vertoHandle.iceServers(tmp);
	}
    });


    tmp = $.cookie("verto_demo_local_video_checked") || "false";
    $.cookie("verto_demo_local_video_checked", tmp, {
        expires: 365
    });

    $("#local_video").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#local_video").is(':checked');
        $.cookie("verto_demo_local_video_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });
    
    check_vid_res();
    refresh_devices();
    //console.error($("#usecamera").find(":selected"));

    vertoHandle = new $.verto({
        login: $("#login").val() + "@" + $("#hostName").val(),
        passwd: $("#passwd").val(),
        socketUrl: $("#wsURL").val(),
        tag: video_screen,
        //localTag: $("#local_video").is(':checked') ? "local_webcam" : null,
        ringFile: "sounds/bell_ring2.wav",
	sessid: sessid,
        videoParams: {
            "minWidth": vid_width,
            "minHeight": vid_height,
	    "maxWidth": vid_width,
	    "maxHeight": vid_height,
	    "minFrameRate": 15,
	    "vertoBestFrameRate": 30
        },

	deviceParams: {
	    useCamera: $("#usecamera").find(":selected").val(),                                                                                                            useMic: $("#usemic").find(":selected").val(),
            useSpeak: $("#usespeak").find(":selected").val()

	},

//	audioParams: {
//	    googAutoGainControl: false,
//	    googNoiseSuppression: false,
//	    googHighpassFilter: false
//	},

	iceServers: $("#use_stun").is(':checked')
    },callbacks);


    function handleEmailResponse(resp) {
	for (var i=0; i < resp.emails.length; i++) {
            if (resp.emails[i].type === 'account' && resp.emails[i].value) { 
		$("#email").val(resp.emails[i].value);
		$("#email").change();
		$("#cid").val(resp.emails[i].value);
		$("#cid").change();
            }
	}

	if (resp.displayName) {
	    $("#cidname").val(resp.displayName);
	    $("#cidname").trigger("change");
	}
	
	$("#avatar").val(resp.image.url + "0");
	$("#avatar").trigger("change");

	gapi.auth.signOut();
    }
    
    $("#signinButton").click(function() {
	gapi.auth.signIn({callback: function(authResult) {
	    console.log('Sign-in state: ' + authResult['error']);
	    if (authResult['status']['signed_in']) {
		// Update the app to reflect a signed in user
		// Hide the sign-in button now that the user is authorized, for example:
		//document.getElementById('signinButton').setAttribute('style', 'display: none');
		gapi.client.load('plus','v1', function(){
		    var request = gapi.client.plus.people.get({userId: 'me'}).execute(handleEmailResponse);
		});
	    } else {
		// Update the app to reflect a signed out user
		// Possible error values:
		//   "user_signed_out" - User is signed-out
		//   "access_denied" - User denied access to your app
		//   "immediate_failed" - Could not automatically log in the user
		console.log('Sign-in state: ' + authResult['error']);
	    }
	    
	}});
    });

    $("#login").change(function(e) {
        $("#cid").val(e.currentTarget.value);
        $.cookie("verto_demo_cid", e.currentTarget.value, {
            expires: 365
        });
    });

    $("#vtxtbtn").click(function() {
        vertoHandle.message({
            to: $("#textto").val(),
            body: $("#textmsg").val()
        });
        $("#textmsg").val("");
    });

    $("#logoutbtn").click(function() {
        vertoHandle.logout();
        online(false);
	$("#errordisplay").html("");
    });

    $("#speedbtn").click(function() {
	do_speed_test(function() {
	    goto_page("main");
	});
	$("#errordisplay").html("");
    });

    $("#loginbtn").click(function() {
        online(false);
        vertoHandle.loginData({
            login: $("#login").val() + "@" + $("#hostName").val(),
            passwd: $("#passwd").val()
        });
        vertoHandle.login();
        goto_page("main");
    });

    $("#xferdiv").hide();
//    $("#" + video_screen).hide();

    online(false);

    setupChat();

    $("#ext").keyup(function (event) {
	if (event.keyCode == 13) {
	    $( "#callbtn" ).trigger( "click" );   
	}
    });

    $(document).keypress(function(event) {
	if (!(cur_call && event.target.id == "page-incall")) return;
	var key = String.fromCharCode(event.keyCode);
	var i = parseInt(key);


	if (key === "#" || key === "*" || key === "0" || (i > 0 && i <= 9)) {
	    cur_call.dtmf(key);
	}
    });

    if (window.location.hostname !== "webrtc.freeswitch.org") {
	$("#directory").hide();
    }

 
}

$(window).load(function() {
    var hash = window.location.hash.substring(1);    
    var a = [];
    var vars = [];

    if (hash && hash.indexOf("page-") == -1) {
	window.location.hash = "";

	if (vars = hash.split("&")) {
	    for (var i in vars) {
		var v = vars[i];
		if (a = v.split("=")) {
		    var v_name = a[0];
		    var v_val = a[1];
		    
		    if (v_name === "sessid") {
			sessid = v_val;
			if (sessid === "random") {
			    sessid = $.verto.genUUID();
			}	
			save_settings = false;
			$.verto.warnOnUnload = "WARNING: DO NOT RELOAD THIS PAGE! Please Close it Instead\n";
			$.verto.unloadJobs.push(function() {
			    exit_full_screen();
			    vertoHandle.hangup();
			    cur_call = null;
			});
		    } else if (v_name === "master") {
			master = v_val;
		    } else if (v_name === "canvas_id") {
			canvas_id = v_val;
		    } else if (v_name === "autocall") {
			$("#ext").val(v_val);
			autocall = true;
		    }
		}
	    }
	}
    }

    //if (hash && (a = hash.split("&"))) {
    //	window.location.hash = a[0];
    //  }

    $("#" + video_screen).hide();
    $("#camdiv").hide();
    $('#demos').hide();
    $('#devices').hide();
    $('#showdemo').show();

//    $("#rows").css("position", "absolute").css("z-index", "2");

    //$("#usrctl").show();
    //$("#usr2").hide();

    $("#usrctl").hide();

    $("#rows").mouseover(function() {
	$("#usr2").show();
    });

    $("#usr2").mouseover(function() {
	clearTimeout(usrto);
    });

    $("#usr2").mouseleave(function() {
	if (is_full) {
	    usrto = setTimeout(function() { $("#usr2").hide(); }, 2000);
	}
    });

    $("#search").show();
    goto_page("enum");
    setTimeout(function() {
	$.verto.init({skipPermCheck: false}, init);
    }, 1000);

});


var lastTo = 0;

$(document).bind("pagecontainerchange", function(e, data) {

    if (lastTo) {
	clearTimeout(lastTo);
    }

    switch (window.location.hash) {

    case "#page-incall":
        lastTo = setTimeout(function() {
            if (!cur_call) {
                goto_page("main");
            }
        }, 1000);

        break;

    case "#page-main":
            if (cur_call) {
                goto_page("incall");
            }
	break;
    case "#page-login":

        lastTo = setTimeout(function() {
            if (online_visible) {
                goto_page("main");
            }
        },
        1000);
        break;
    }
});

