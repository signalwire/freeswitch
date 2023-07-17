const caller_id = session.getVariable("caller_id_number");
const destination_number = session.getVariable("destination_number");
const domain_name = session.getVariable("domain_name");

// Custom Header send during accept (store onAccept)
session.execute("set", "sip_rh_X-Reason=" + destination_number);
// Custom Header send during reject (store onReject)
session.execute("set", "sip_bye_h_X-Reason=Not My problem");
// Sends a custom headder !!! Needs to be sip_h_X-TheNameOfTheHeader
session.setVariable("sip_h_X-TestHeadder", "Detta är ett test skickat från fs");

console_log(
  "console",
  caller_id + " called " + destination_number + domain_name + "\n"
);

session.execute("set", "hangup_after_bridge=true");
session.execute(
  "bridge",
  "user/" +
    destination_number +
    "-webb@" +
    domain_name +
    ", user/" +
    destination_number +
    "-phone@" +
    domain_name
);
session.execute("sleep", "2000");
// console_log("CONSOLE", JSON.stringify(session, null, 4))
// const allProps = Object.getOwnPropertyNames(session);
// console_log("CONSOLE", JSON.stringify(allProps, null, 4))
// // loop through all props and log them, if its a function log the function code
// for (const prop of allProps) {
//     if (typeof session[prop] === 'function') {
//         console_log("CONSOLE", "Function: " + prop + " = " + session[prop].toString() + "\n");
//     } else {
//         console_log("CONSOLE", "Property: " + prop + " = " + session[prop] + "\n");
//     }
// }

// const test = session.getVariable("sip_h_X-TestHead");
// console_log("CONSOLE", "TEST HEEEADER: " + test + "\n");
// const test2 = session.getVariable("X-TestHead");
// console_log("CONSOLE", "TEST2 HEEEADER: " + test2 + "\n");

// const tes2t = session.dumpENV("text");
// console_log("CONSOLE", "TEST HEEEADER 22222222222: " + JSON.stringify(tes2t, null, 4) + "\n");

session.execute("playback", "ivr/ivr-extension_number.wav");
session.execute("phrase", "voicemail_say_number," + destination_number);
session.execute("playback", "voicemail/vm-not_available.wav");
session.execute("sleep", "2000");
