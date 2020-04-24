This is a module to recognize speech using Vosk server. You can run the server in docker with simple:

```
docker run -d -p 2700:2700 alphacep/kaldi-en:latest
```

See for more details https://github.com/alphacep/vosk-server

To use this server with freeswitch:

  1. Make sure you have libks installed
  1. Configure and install freeswitch including `mod_vosk.so`
  1. Make sure mod_vosk.so is enabled in `modules.conf.xml` and `conf/vosk.conf.xml` is placed in autoload_configs

Run the following sample dialplan:

```
<include>
  <context name="default">
    <extension name="asr_demo">
        <condition field="destination_number" expression="^.*$">
          <action application="answer"/>
          <action application="play_and_detect_speech" data="ivr/ivr-welcome.wav detect:vosk default"/>
          <action application="speak" data="tts_commandline|espeak|You said ${detect_speech_result}!"/>
        </condition>
    </extension>
  </context>
</include>
```
