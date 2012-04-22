-- Copyright (c) 2011-2012, Travis Cross.
--
-- The contents of this file are subject to the Mozilla Public License
-- Version 1.1 (the "License"); you may not use this file except in
-- compliance with the License. You may obtain a copy of the License
-- at http://www.mozilla.org/MPL/
--
-- Software distributed under the License is distributed on an "AS IS"
-- basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
-- the License for the specific language governing rights and
-- limitations under the License.
--
-- zrtp_proxy_media.lua
-- 
-- The logic in this script enables ZRTP sessions to negotiate
-- end-to-end security associations, which is desirable whether or not
-- the switch natively supports ZRTP itself.
-- 
-- To enable this logic, call the script from the top of your dialplan
-- as so:
-- 
--   <extension name="global" continue="true">
--     <condition break="never">
--       <action application="lua" data="lua/zrtp_proxy_media.lua"/>
--     </condition>
--   </extension>
-- 
-- If any particular call flow should never have proxy_media enabled,
-- such as for connecting to voicemail systems or conferences, make
-- sure this is called before the bridge:
-- 
--   <action application="lua" data="lua/zrtp_proxy_media.lua disable"/>

api=freeswitch.API()

function sappend(s1,s2) if s1 and #s1>0 then return s1..s2 else return s2 end end
function log(level,msg) return freeswitch.consoleLog(level,msg.."\n") end
function ready() return session:ready() end
function getvar(var) return session:getVariable(var) end
function getvarp(var) return getvar(var)=="true" end
function setvar_a(k,v) return session:setVariable(k,v) end
function append_var(k,v) return setvar_a(k,sappend(getvar(k),v)) end
function export(k) return append_var("export_vars",","..k) end
function setvar_ab(k,v) if v then setvar_a(k,v) end return export(k) end
function setvar_b(k,v) return setvar_ab("nolocal:"..k,v) end

function enable_zd(msg)
  log("info",msg)
  setvar_ab("zrtp_set","true")
  setvar_ab("proxy_media","true")
  setvar_ab("zrtp_secure_media","false")
end

function disable_zd(msg)
  log("info",msg)
  setvar_ab("zrtp_set","true")
  setvar_ab("proxy_media","false")
  setvar_ab("zrtp_secure_media","true")
end

function xfer(x)
  return session:transfer(x,getvar("dialplan"),getvar("context"))
end

function main()
  if ready() then
    session:setAutoHangup(false)
    local dst=getvar("destination_number")
    if argv[1]=="disable" then
      return disable_zd("zrtp-direct disabled on this call flow")
    elseif getvarp("zrtp_set") then
      return log("notice","zrtp already decided; doing nothing") end
    local x=dst:match("^%*%*%*(.*)$")
    if x then
      enable_zd("going zrtp-direct based on star code")
      return xfer(x) end
    local x=dst:match("^%*%*(.*)$")
    if x then
      disable_zd("going zrtp-indirect based on star code")
      return xfer(x) end
    if getvar("switch_r_sdp"):match("a=zrtp%-hash:") then
      return enable_zd("going zrtp-direct based on a=zrtp-hash") end
    return disable_zd("not going zrtp-direct")
  end
end

main()
