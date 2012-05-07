local human_detected = false;
local voicemail_detected = false;

function onInput(session, type, obj)
    if type == "dtmf" and obj['digit'] == '1' and human_detected == false then
        human_detected = true;
        return "break";
    end

    if type == "event" and voicemail_detected == false then
        voicemail_detected = true;
        return "break";
    end
end

session:setInputCallback("onInput");
session:execute("vmd","start");
