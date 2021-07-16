session:answer();

while session:ready() do
    session:execute("play_and_detect_speech", "ivr/ivr-welcome.wav detect:vosk default")
    local res = session:getVariable('detect_speech_result')
    if res ~= nil then
        session:execute("speak", "tts_commandline|espeak|You said " .. session:getVariable("detect_speech_result"))
    else
        session:execute("speak", "tts_commandline|espeak|You said nothing");
    end
end

session:hangup()
