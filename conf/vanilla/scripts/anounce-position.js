let caller_uuid = argv[0];
let queue_name = argv[1];
let timer = argv[2];

if (caller_uuid === null || queue_name === null || timer === null) {
  console_log("CONSOLE", "empty" + "\n");
}

while (true) {
  apiExecute("sleep", timer * 1000);
  members = apiExecute("callcenter_config queue list members " + queue_name);
  console_log("CONSOLE", caller_uuid + "\n");
  console_log("CONSOLE", caller_uuid + "\n");
  let pos = 1;
  let exit = false;
  break;
}

exit();
