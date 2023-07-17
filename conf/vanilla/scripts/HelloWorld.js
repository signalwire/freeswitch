var languageCode = "en";
var soundDir = "sound/";

var caller_id = session.getVariable("caller_id_number");
var destination_number = session.getVariable("destination_number");

function playFile(fileName, callBack, callBackArgs) {
    session.streamFile("misc/" + fileName, callBack, callBackArgs);
}

console_log(
    "CONSOLE",
    "Destination number: (World)" + destination_number + "\n"
);

session.answer();
session.streamFile("misc/longer_hold_times.wav");

//session.execute("bridge", "user/1001");

//session.execute("socket", "127.0.0.1:8085 async full");
exit();
// <action application="javascript" data="Router.js" />
