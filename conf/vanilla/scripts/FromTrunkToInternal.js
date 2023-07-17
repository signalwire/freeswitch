use("CURL");

const curl = new CURL();
const destination_number = session.getVariable("destination_number");
const domain_name = session.getVariable("domain_name");

function userInternalNumber(data) {
  console_log("console", data + "\n");

  if (!data || data === "No internal number found") {
    return "noData";
  }

  return data;
}

let res = curl.run(
  "GET",
  "http://localhost:3000/user/externalToInternal",
  "destinationNumber=" + destination_number,
  userInternalNumber,
  "",
  ""
);

if (res !== "noData") {
  session.execute(
    "bridge",
    "user/" +
      res +
      "-webb@" +
      domain_name +
      ",user/" +
      res +
      "-phone@" +
      domain_name
  );
}
session.answer();
session.execute("playback", "ivr/ivr-extension_number.wav");
session.execute("playback", "voicemail/vm-not_available.wav");
session.execute("sleep", "2000");

session.hangup();
