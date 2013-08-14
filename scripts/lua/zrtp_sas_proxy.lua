-- zrtp_sas_proxy.lua
--
-- Copyright (c) 2011-2013 Travis Cross
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--
--
-- When we're acting as a ZRTP man-in-the-middle, proxy the SAS (Short
-- Authentication String) from one leg of the call to the other.
--
-- This script should be called asynchonously with luarun.  e.g.:
--
-- <action application="export" data="nolocal:api_on_answer=luarun zrtp_sas_proxy.lua ${uuid}"/>
--
aleg=argv[1]
api=freeswitch.API()

function log(level,msg) return freeswitch.consoleLog(level,"zrtp_sas: "..msg.."\n") end
function sleep(sec) return freeswitch.msleep(sec*1000) end
function ready() return api:execute("uuid_exists",aleg)=="true" end
function getvar(uuid,var)
  local x=api:execute("uuid_getvar",uuid.." "..var)
  if x=="_undef_" then return nil end
  return x
end
function getvarp(uuid,var) return getvar(uuid,var)=="true" end
function display(uuid,msg)
  local cidn=getvar(uuid,"caller_id_name")
  return api:execute("uuid_display",uuid.." "..msg.." "..cidn)
end

function mk_sas(sas1,sas2)
  if sas1 and sas2 then return sas1.." "..sas2
  else return sas1 or sas2 or "" end
end

function get_sas(uuid)
  return mk_sas(getvar(uuid,"zrtp_sas1_string_audio"),
                getvar(uuid,"zrtp_sas2_string"))
end

function log_sas(leg,uuid)
  return log("notice",leg..": "..uuid.." sas: "..get_sas(uuid))
end

function display_sas(to,from)
  return display(to," ("..get_sas(from)..")")
end

function get_bleg(aleg)
  local retries=15 bleg=nil
  while ready() do
    if retries<1 then return nil end
    local bleg=getvar(aleg,"signal_bond")
    if bleg then return bleg end
    log("debug","waiting for bleg uuid...")
    sleep(1)
    retries=retries-1
  end
end

function handle_sas(aleg,bleg)
  local retries=45 af=false bf=false
  while ready() do
    if retries<1 then return nil end
    if not af and getvarp(aleg,"zrtp_secure_media_confirmed_audio") then
      af=true
      log_sas("aleg",aleg)
      display_sas(bleg,aleg)
    end
    if not bf and getvarp(bleg,"zrtp_secure_media_confirmed_audio") then
      bf=true
      log_sas("bleg",bleg)
      display_sas(aleg,bleg)
    end
    if (af and bf) then break
    elseif af then log("debug","waiting on bleg zrtp...")
    elseif bf then log("debug","waiting on aleg zrtp...")
    else log("debug","waiting for zrtp...") end
    sleep(1)
    retries=retries-1
  end
end

if not (getvarp(aleg,"zrtp_passthru") or getvarp(aleg,"proxy_media")) then
  handle_sas(aleg,get_bleg(aleg))
end
