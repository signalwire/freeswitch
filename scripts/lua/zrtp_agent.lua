-- ZRTP Enrollment Agent
session:setVariable("zrtp_secure_media", "true");
session:setVariable("zrtp_enrollment", "true");
session:sleep(100);
session:answer();
session:streamFile("zrtp/zrtp-status_securing.wav");
session:sleep(3000);
-- Give the agent time to bring up ZRTP.

local zrtp_secure_media_confirmed = session:getVariable("zrtp_secure_media_confirmed_audio");
local zrtp_new_user_enrolled = session:getVariable("zrtp_new_user_enrolled_audio");
local zrtp_already_enrolled = session:getVariable("zrtp_already_enrolled_audio");

if zrtp_secure_media_confirmed == "true" then
   session:streamFile("zrtp/zrtp-status_secure.wav");
else
   session:streamFile("zrtp/zrtp-status_notsecure.wav");
end

session:streamFile("zrtp/zrtp-enroll_welcome.wav");
session:sleep(1000);

if zrtp_secure_media_confirmed == "true" then
   if zrtp_new_user_enrolled == "true" then 
      session:streamFile("zrtp/zrtp-enroll_confirmed.wav");
      session:sleep(3000);
   else
      if zrtp_already_enrolled == "true" then 
	 session:streamFile("zrtp/zrtp-enroll_already_enrolled.wav");
      end 
   end
else 
   session:streamFile("zrtp/zrtp-enroll_notzrtp.wav");
end

session:sleep(1000);
session:streamFile("zrtp/zrtp-thankyou_goodbye.wav");
session:sleep(1000);
