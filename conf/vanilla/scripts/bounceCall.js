const args = argv;
function hangup_function_name() {
  console_log("console", "Hangup function called\n");
  exit();
}
console_log("console", "It Works !!!!!!" + args[0] + "\n");

result = session.setHangupHook(hangup_function_name);
session.answer();
session.execute("bridge", "sofia/gateway/Trunk/+" + args[0]);
