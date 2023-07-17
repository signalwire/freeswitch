// This works : https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/Client-and-Developer-Interfaces/JavaScript/JavaScript-API-Reference/Run_13173558/

// if (session.ready()) {
//     session.answer();

// function my_callback(string, arg) {
//     console_log("info", string);
//     console_log("Args", arg);
//     return true;
// }
//     var curl = new CURL();

//     curl.run("GET", "https://pokeapi.co/api/v2/pokemon/ditto", "", my_callback, "TEST", "");

//     session.hangup();
// }

session.setHangupHook(onHangup);
const caller_id = session.getVariable("caller_id_number");
const destination_number = session.getVariable("destination_number");
const domain_name = session.getVariable("domain_name");

console_log("console", caller_id + " called " + destination_number + "\n");

if (/^(\d{4})$/.test(destination_number)) {
  session.execute("javascript", "InternalCall.js");
} else if (/^123456$/.test(destination_number)) {
  session.execute("javascript", "IVR.Test.js");
} else if (/^\+4685175828[4-9]$/.test(destination_number)) {
  session.execute("javascript", "ExternalCall.js");
  // session.execute("javascript", "FromTrunkToInternal.js");
} else if (/^10000$/.test(destination_number)) {
  session.execute("bridge", "user/5000@" + domain_name);

  /* session.execute("playback", "ivr/ivr-welcome.wav");
    session.execute("playback", "ivr/ivr-one_moment_please.wav");
    session.execute("set", "hangup_after_bridge=true");
    //Runs a lua script. (Copy paste from documentation)
    session.execute(
        "set",
        "result=${luarun(callcenter-announce-position.lua ${uuid} sales@default 35)}"
        ); */

  /*  session.execute(
        "set",
        "result=${jsrun(anounce-position.js ${uuid} sales@default 10)}"
        ); */
  /*  session.execute("callcenter", "sales@default");  */
} else if (/^\+?\d{10,11}$/.test(destination_number)) {
  session.execute("javascript", "ExternalCall.js");
  // session.execute("bridge", "sofia/gateway/Trunk/+" + destination_number);
}

exit();

function onHangup(session, how) {
  // let test = session.getVariable("X-TestHead");
  // console_log("console", how + " HOOK" + " name: " + session.name + " cause: " + session.cause + "\n");

  // console_log("console", "Testiiiink:" + JSON.stringify(argv[0]) + "\n");

  // console_log("console", "Test:" + test + "\n");

  exit();
}
