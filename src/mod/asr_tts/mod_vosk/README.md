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

!!!! ATTENTION, for reliable work this module requires several fixes in libks which are not yet merged, so rebuild libks from this:

```
git clone --branch vosk-fix --single-branch https://github.com/alphacep/libks
```

You can create more advanced dealplans with ESL and scripts in various languages. See examples in scripts folder.

!!! ATTENSION In order for ESL to recieve events, make sure that fire_asr_events variable is set to true (false by default).
The dialplan can look like this:

```
<include>
  <context name="default">
    <extension name="asr_demo">
        <condition field="destination_number" expression="^.*$">
          <action application="answer"/>
          <action application="set" data="fire_asr_events=true"/>
          <action application="detect_speech" data="vosk default default"/>
          <action application="sleep" data="10000000"/>
        </condition>
    </extension>
  </context>
</include>
```
