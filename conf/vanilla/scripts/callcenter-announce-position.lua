-- Inte min kod!! /Markus
-- callcenter-announce-position.lua
-- Announce queue position to a member in a given mod_callcenter queue.
-- Arguments are, in order: caller uuid, queue_name, interval (in milliseconds).
api = freeswitch.API()
caller_uuid = argv[1]
queue_name = argv[2]
sek = argv[3]
if caller_uuid == nil or queue_name == nil or sek == nil then
    return
end
while (true) do
    -- Pause between announcements
    freeswitch.msleep(sek*1000)
    members = api:executeString("callcenter_config queue list members "..queue_name)  
    pos = 1
    exists = false
    for line in members:gmatch("[^\r\n]+") do
        if (string.find(line, "Trying") ~= nil or string.find(line, "Waiting") ~= nil) then
            -- Members have a position when their state is Waiting or Trying
            if string.find(line, caller_uuid, 1, true) ~= nil then
                -- Member still in queue, so script must continue
                exists = true
                api:executeString("uuid_broadcast "..caller_uuid.." ivr/ivr-you_are_number.wav aleg")
                api:executeString("uuid_broadcast "..caller_uuid.." digits/"..pos..".wav aleg")
            end
            pos = pos+1
        end
    end
    -- If member was not found in queue, or it's status is Aborted - terminate script
    if exists == false then
        return
    end
end