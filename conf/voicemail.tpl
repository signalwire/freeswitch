From: FreeSWITCH mod_voicemail <${voicemail_account}@${voicemail_domain}>
To: <${voicemail_email}>
Subject: ${voicemail_message_len} sec Voicemail from ${voicemail_caller_id_name} ${voicemail_caller_id_number}
X-Priority: ${voicemail_priority}
X-Mailer: FreeSWITCH

Content-Type: multipart/alternative; 
	boundary=000XXX000

--000XXX000
Content-Type: text/plain; charset=ISO-8859-1; Format=Flowed
Content-Disposition: inline
Content-Transfer-Encoding: 7bit

Created: ${voicemail_time}
From: ${voicemail_caller_id_name} ${voicemail_caller_id_number}
Duration: ${voicemail_message_len}
Account: ${voicemail_account}@${voicemail_domain}

--000XXX000
Content-Type: text/html; charset=ISO-8859-1
Content-Disposition: inline
Content-Transfer-Encoding: 7bit

<font face=arial>
<b>Message From ${voicemail_caller_id_name} ${voicemail_caller_id_number}</b><br>
<hr noshade size=1>
Created: ${voicemail_time}<br>
Duration: ${voicemail_message_len}<br>
Account: ${voicemail_account}@${voicemail_domain}<br>

--000XXX000--
