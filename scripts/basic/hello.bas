
s$ = "hello "
s$ = s$ + "world"

FS_LOG "WARNING" s$ + "!"

FS_EXECUTE "answer"
FS_EXECUTE "sleep" "1000"
FS_EXECUTE "playback" "misc/misc-cluecon_is_premier_conference.wav"


