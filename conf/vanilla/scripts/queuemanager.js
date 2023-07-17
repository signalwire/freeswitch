use("CURL");

const curl = new CURL();
const destination_number = session.getVariable("destination_number");
const origin = session.getVariable("caller_id_number");
const uuid = session.uuid;

let isInQueue = true;
let times = 0;
let disconnectReason = "callAbandoned";

function callStatus(data) {
  if (data !== "2") {
    console_log("console", "Call status: " + data + "\n");
  }
  return data;
}

// console_log("console", "Destination number: " + destination_number + "\n");
// console_log("console", "Origin: " + origin + "\n");
// console_log("console", "UUID: " + uuid + "\n");

session.answer();
session.streamFile("ivr/ivr-welcome.wav");
session.streamFile("C1/_waiting_for_you.wav");

const data = {
  landingNumber: destination_number,
  callerNumber: origin,
  id: uuid,
  queueIdRelation: "123456",
};

let res = curl.run(
  "POST",
  "http://localhost:3000/queue/add",
  "data=" + JSON.stringify(data),
  "",
  "",
  "",
  10
);

while (isInQueue) {
  msleep(500);

  if (!session.ready()) {
    console_log("console", "Session not ready" + "\n");
    isInQueue = false;
    break;
  }

  times++;
  const data = {
    id: uuid,
  };
  let res = curl.run(
    "POST",
    "http://localhost:3000/queue/check",
    "data=" + JSON.stringify(data),
    callStatus,
    "",
    "",
    10
  );

  switch (res) {
    case "0":
    case "2":
      break;
    case "3":
      break;
    case "4":
      session.streamFile("C1/46_Queue_NoAgents.wav");
      disconnectReason = "noLoggedInOrTimeout";
      isInQueue = false;
      break;
    case "5":
      session.streamFile("C1/_waiting_for_you.wav");
      break;
    case "6":
      break;
    case "7":
      break;
    case "8":
      break;
    case "9":
      break;
    default:
      session.execute("bridge", "user/5000-webb");
      break;
  }
}

const disconnectData = {
  id: uuid,
  disconnectReason,
};

const res2 = curl.run(
  "POST",
  "http://localhost:3000/queue/leave",
  "data=" + JSON.stringify(disconnectData),
  "",
  "",
  "",
  10
);
