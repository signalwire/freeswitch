'use strict';
var cur_call = null;
var confMan = null;
var $display = $("#display");
var verto;
var ringing = false;

function display(msg) {
    $("#calltitle").html(msg);
}

function clearConfMan() {
    if (confMan) {
        confMan.destroy();
        confMan = null;
    }

    $("#conf").hide();
}

function goto_dialog(where) {
    $.mobile.changePage("#dialog-" + where, {
        role: "dialog"
    });
}

function goto_page(where) {
    $.mobile.changePage("#page-" + where);
}

var first_login = false;
var online_visible = false;
function online(on) {
    if (on) {
        $("#online").show();
        $("#offline").hide();
        first_login = true;
    } else {
        if (first_login && online_visible) {
            goto_dialog("logout");
        }

        $("#online").hide();
        $("#offline").show();
    }

    online_visible = on;
}

function check_vid() {
    var use_vid = $("#use_vid").is(':checked');
    return use_vid;
}

var callbacks = {

    onMessage: function(verto, dialog, msg, data) {

        switch (msg) {
        case $.verto.enum.message.pvtEvent:
            console.error("pvtEvent", data.pvtData.action);
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

                    break;
                }
            }
            break;
        case $.verto.enum.message.info:
            $("#text").html("Message from: <b>" + data.from + "</b>:<br>" + "<pre>" + data.body + "</pre>");
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
		    useStereo: $("#use_stereo").is(':checked')
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

        case $.verto.enum.state.active:
            display("Talking to: " + d.cidString());
            goto_page("incall");
            break;
        case $.verto.enum.state.hangup:
        case $.verto.enum.state.destroy:
            clearConfMan();
            goto_page("main");
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

            verto.subscribe("presence", {
                handler: function(v, e) {
                    console.error("PRESENCE:", e);
                }
            });
            if (!window.location.hash) {
                goto_page("main");
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
        console.debug("w00t", e);
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

$("#callbtn").click(function() {
    $('#ext').trigger('change');

    if (cur_call) {
        return;
    }

    cur_call = verto.newCall({
        destination_number: $("#ext").val(),
        caller_id_name: $("#name").val(),
        caller_id_number: $("#cid").val(),
        useVideo: check_vid(),
        useStereo: $("#use_stereo").is(':checked')
    });
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

    verto = new $.verto({
        login: $("#login").val() + "@" + $("#hostName").val(),
        passwd: $("#passwd").val(),
        socketUrl: $("#wsURL").val(),
        tag: "webcam",
        ringFile: "sounds/bell_ring2.wav",
        videoParams: {
            "minWidth": "1280",
            "minHeight": "720"
        }
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
        goto_page("login");
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
}

$(document).ready(function() {
    init();
    $("#page-incall").on("pagebeforechange", function(event) {});
});

$(document).bind("pagebeforechange", function(e, data) {
    if (typeof(data.toPage) !== "string") {
        return;
    }

    switch (window.location.hash) {

    case "#page-incall":

        console.error(e, data);
        setTimeout(function() {
            if (!cur_call) {
                goto_page("main");
            }
        },
        10000);
        break;

    case "#page-main":

        console.error(e, data);
        setTimeout(function() {
            if (cur_call && !ringing) {
                goto_page("incall");
            }
        },
        2000);
        break;

    case "#page-login":

        setTimeout(function() {
            if (online_visible) {
                goto_page("main");
            }
        },
        1000);
        break;
    }
});
