local https = require("socket.http")
local ip = os.getenv("LOCAL_IPV4")
local response_body = {}
-- jitter buffer stats
local size_max_ms = session:getVariable("rtp_jb_size_max_ms");
local size_est_ms = session:getVariable("rtp_jb_size_est_ms");
local acceleration_ms = session:getVariable("rtp_jb_acceleration_ms");
local expand_ms = session:getVariable("rtp_jb_expand_ms");
local jitter_max_ms = session:getVariable("rtp_jb_jitter_max_ms");
local jitter_est_ms = session:getVariable("rtp_jb_jitter_est_ms");

local reset_count = session:getVariable("rtp_jb_reset_count"); 
local reset_too_big = session:getVariable("rtp_jb_reset_too_big");
local reset_missing_frames = session:getVariable("rtp_jb_reset_missing_frames");
local reset_ts_jump = session:getVariable("rtp_jb_reset_ts_jump");
local reset_error = session:getVariable("rtp_jb_reset_error");
local call_id     = session:getVariable("sip_call_id");
local out_call_id = session:getVariable("last_bridge_to");

if size_max_ms == nil or size_est_ms == nil or acceleration_ms == nil or expand_ms == nil or jitter_max_ms == nil or jitter_est_ms == nil then
	session:consoleLog("info",  "[metrics] jitter no data\n");
	return
end
local request_body = '{"in_call_id": "'..call_id..'", "out_call_id": "'..out_call_id..'", "jb":{"size_max_ms":'..size_max_ms..
                      ',"size_est_ms":'..size_est_ms..',"acceleration_ms":'..acceleration_ms..',"expand_ms":'..expand_ms..
		      ',"jitter_max_ms":'..jitter_max_ms..',"jitter_est_ms":'..jitter_est_ms..',"reset":'..reset_count
-- if reset_too_big ~= "0" then
	request_body = request_body .. ',"reset_too_big":'..reset_too_big
-- end
if reset_missing_frames ~= "0" then
	request_body = request_body .. ',"reset_missing_frames":'..reset_missing_frames
end
if reset_ts_jump ~= "0" then
	request_body = request_body .. ',"reset_ts_jump":'..reset_ts_jump
end
if reset_error ~= "0" then
	request_body = request_body .. ',"reset_error":'..reset_error
end

local v = request_body .. '}}';

local r, c, h, s = https.request{
	method = 'POST',
	url = "http://"..ip..":80/freeswitch_metrics",
	headers = {
		["Content-Type"] = "application/json",
		["Content-Length"] = string.len(v)
	},
	source = ltn12.source.string(v),
	sink = ltn12.sink.table(response_body)
}
-- print('statusCode ', c)
session:consoleLog("info",  "[metrics] jitter:".. v .. "\n");
