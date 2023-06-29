var caller_id = session.getVariable("caller_id_number");
var destination_number = session.getVariable("destination_number");
var domain_name = session.getVariable("domain_name");

console_log("CONSOLE", "Destination number:" + destination_number + "\n");
console_log("CONSOLE", "domain_name:" + domain_name + "\n");
console_log("CONSOLE", "caller_id: " + caller_id + "\n");

if (/^(\d{4})$/.test(destination_number)) {
  console_log(
    "CONSOLE",
    "user/" + destination_number + "@" + domain_name + "\n"
  );
  session.execute("bridge", "user/" + destination_number + "@" + domain_name);
}
if (/^123456$/.test(destination_number)) {
  session.execute("javascript", "HelloWorld.js");
}
if (/^10000$/.test(destination_number)) {
  session.execute("playback", "ivr/ivr-welcome.wav");
  session.execute("playback", "ivr/ivr-one_moment_please.wav");
  session.execute("set", "hangup_after_bridge=true");
  //Runs a lua script. (Copy paste from documentation)
  session.execute(
    "set",
    "result=${luarun(callcenter-announce-position.lua ${uuid} sales@default 35)}"
  );

  /*  session.execute(
    "set",
    "result=${jsrun(anounce-position.js ${uuid} sales@default 10)}"
  ); */
  session.execute("callcenter", "sales@default");
}
if (/^(\d{11})$/.test(destination_number)) {
  session.execute("bridge", "sofia/gateway/Trunk/+" + destination_number);
}
exit();
