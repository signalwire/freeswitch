const destination_number = session.getVariable("destination_number");

const headerValue = session.getVariable("sip_i_x_NumberToShow");
if (headerValue) {
  session.execute("set", "effective_caller_id_number=" + headerValue);
}

session.execute("set", "effective_caller_id_name=Custommer First");
session.execute("bridge", "sofia/gateway/Trunk/+" + destination_number);
