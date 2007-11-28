From: FreeSWITCH mod_voicemail <${voicemail_account}@${voicemail_domain}>
To: <${voicemail_email}>
Subject: ${voicemail_message_len} sec Voicemail from ${voicemail_caller_id_name} ${voicemail_caller_id_number}
X-Priority: ${voicemail_priority}
X-Mailer: FreeSWITCH

Content-Type: multipart/alternative; 
	boundary=000XXX000

--000XXX000
Content-Type: text/plain; charset=ISO-8859-1; Format=Flowed
Content-Disposition: attachment
Content-Transfer-Encoding: 7bit

At ${voicemail_time} you were left a ${voicemail_message_len} second message from ${voicemail_caller_id_name} ${voicemail_caller_id_number}
to your account ${voicemail_account}@${voicemail_domain}

--000XXX000
Content-Type: text/html; charset=ISO-8859-1
Content-Disposition: attachment
Content-Transfer-Encoding: 7bit

At ${voicemail_time} you were left a ${voicemail_message_len} second message from ${voicemail_caller_id_name} ${voicemail_caller_id_number}
to your account ${voicemail_account} @ ${voicemail_domain} <a href=tel:${voicemail_caller_id_number}>Click to call</a>

--000XXX000--
