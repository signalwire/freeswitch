'use strict';
var cur_call = null;
var confMan = null;
var verto;
var ringing = false;
var autocall = false;
var chatting_with = false;

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
	if (!cur_call && chatting_with) {
	    return;
	}

	cur_call.message({to: chatting_with, 
			  body: $("#chatmsg").val(), 
			  from_msg_name: cur_call.params.caller_id_name, 
			  from_msg_number: cur_call.params.caller_id_number
			 });  
	$("#chatmsg").val("");
    });

    $("#chatmsg").keyup(function (event) {
	if (event.keyCode == 13 && !event.shiftKey) {
	    $( "#chatsend" ).trigger( "click" );   
	}
    });

}

function check_vid() {
    var use_vid = $("#use_vid").is(':checked');
    return use_vid;
}

var callbacks = {

    onMessage: function(verto, dialog, msg, data) {

        switch (msg) {
        case $.verto.enum.message.pvtEvent:
//            console.error("pvtEvent", data.pvtData);
            if (data.pvtData) {
                switch (data.pvtData.action) {

                case "conference-liveArray-part":
                    clearConfMan();
                    break;
                case "conference-liveArray-join":
                    clearConfMan();
		    confMan = new $.verto.confMan(verto, {
			tableID: "#conf_list",
			statusID: "#conf_count",
			mainModID: "#conf_mod",
			displayID: "#conf_display",
			dialog: dialog,
			hasVid: check_vid(),
			laData: data.pvtData
		    });

                    $("#conf").show();
		    $("#chatwin").html("");
                    $("#message").show();

		    chatting_with = data.pvtData.chatID;

                    break;
                }
            }
            break;
        case $.verto.enum.message.info:
	    var body = data.body;

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
        cur_call = d;

	if (d.state == $.verto.enum.state.ringing) {
	    ringing = true;
	} else {
	    ringing = false;
	}

        switch (d.state) {
        case $.verto.enum.state.ringing:
            display("Call From: " + d.cidString());

            $("#ansbtn").click(function() {
                cur_call.answer({
		    useStereo: $("#use_stereo").is(':checked'),
		    callee_id_name: $("#name").val(),
		    callee_id_number: $("#cid").val(),
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
			useStereo: $("#use_stereo").is(':checked')
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
            display("Talking to: " + d.cidString());
            goto_page("incall");
            break;
        case $.verto.enum.state.hangup:
	    $("#main_info").html("Call ended with cause: " + d.cause);
            goto_page("main");
        case $.verto.enum.state.destroy:
	    $("#hangup_cause").html("");
            clearConfMan();

            cur_call = null;
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
            online(true);

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
        } else {
            goto_page("login");
            goto_dialog("login-error");
        }

    },
    onWSClose: function(v, success) {
        if ($('#online').is(':visible')) {
            display("");
            online(false);
        }
        var today = new Date();
        $("#errordisplay").html("Connection Error.<br>Last Attempt: " + today);
        goto_page("main");
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
    verto.hangup();
    cur_call = null;
});

$("#webcam").click(function() {
    check_vid();
});

function docall() {
    $('#ext').trigger('change');

    if (cur_call) {
        return;
    }

    $("#main_info").html("Trying");

    cur_call = verto.newCall({
        destination_number: $("#ext").val(),
        caller_id_name: $("#name").val(),
        caller_id_number: $("#cid").val(),
        useVideo: check_vid(),
        useStereo: $("#use_stereo").is(':checked')
    });
}

$("#callbtn").click(function() {
    docall();
});

function pop(id, cname, dft) {
    var tmp = $.cookie(cname) || dft;
    $.cookie(cname, tmp, {
        expires: 365
    });
    $(id).val(tmp).change(function() {
        $.cookie(cname, $(id).val(), {
            expires: 365
        });
    });
}



function init() {
    cur_call = null;

    pop("#ext", "verto_demo_ext", "3500");
    pop("#name", "verto_demo_name", "FreeSWITCH User");
    pop("#cid", "verto_demo_cid", "1008");
    pop("#textto", "verto_demo_textto", "1000");

    pop("#login", "verto_demo_login", "1008");
    pop("#passwd", "verto_demo_passwd", "1234");

    pop("#hostName", "verto_demo_hostname", window.location.hostname);
    pop("#wsURL", "verto_demo_wsurl", "wss://" + window.location.hostname + ":8082");

    var tmp = $.cookie("verto_demo_vid_checked") || "false";
    $.cookie("verto_demo_vid_checked", tmp, {
        expires: 365
    });

    $("#use_vid").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_vid").is(':checked');
        $.cookie("verto_demo_vid_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });

    tmp = $.cookie("verto_demo_stereo_checked") || "false";
    $.cookie("verto_demo_stereo_checked", tmp, {
        expires: 365
    });

    $("#use_stereo").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_stereo").is(':checked');
        $.cookie("verto_demo_stereo_checked", tmp ? "true" : "false", {
            expires: 365
        });
    });

    tmp = $.cookie("verto_demo_stun_checked") || "false";
    $.cookie("verto_demo_stun_checked", tmp, {
        expires: 365
    });

    $("#use_stun").prop("checked", tmp === "true").change(function(e) {
        tmp = $("#use_stun").is(':checked');
        $.cookie("verto_demo_stun_checked", tmp ? "true" : "false", {
            expires: 365
        });
	if (verto) {
	    verto.iceServers(tmp);
	}

	alert(tmp);
    });

    verto = new $.verto({
        login: $("#login").val() + "@" + $("#hostName").val(),
        passwd: $("#passwd").val(),
        socketUrl: $("#wsURL").val(),
        tag: "webcam",
        ringFile: "sounds/bell_ring2.wav",
        videoParams: {
            "minWidth": "1280",
            "minHeight": "720"
        },
	iceServers: $("#use_stun").is(':checked')
    },callbacks);

    $("#login").change(function(e) {
        $("#cid").val(e.currentTarget.value);
        $.cookie("verto_demo_cid", e.currentTarget.value, {
            expires: 365
        });
    });

    $("#vtxtbtn").click(function() {
        verto.message({
            to: $("#textto").val(),
            body: $("#textmsg").val()
        });
        $("#textmsg").val("");
    });

    $("#logoutbtn").click(function() {
        verto.logout();
        online(false);
    });

    $("#loginbtn").click(function() {
        online(false);
        verto.loginData({
            login: $("#login").val() + "@" + $("#hostName").val(),
            passwd: $("#passwd").val()
        });
        verto.login();
        goto_page("main");
    });

    $("#xferdiv").hide();
    $("#webcam").hide();

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

$(document).ready(function() {
    var hash = window.location.hash.substring(1);    
    var a = [];

    if (hash && hash.indexOf("page-") == -1) {
	window.location.hash = "";
	$("#ext").val(hash);
	autocall = true;
    }

    if (hash && (a = hash.split("&"))) {
	window.location.hash = a[0];
    }

    init();

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

