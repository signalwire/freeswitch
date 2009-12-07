#!/bin/bash
sounds_en_us_callie="freeswitch-sounds-en-us-callie-48000-1.0.12.tar.gz"
sounds_music="freeswitch-sounds-music-48000-1.0.8.tar.gz"
sounds_ru_RU_elena="freeswitch-sounds-ru-RU-elena-48000-1.0.12.tar.gz"

cd freeswitch-sounds-music
if [ ! -f $sounds_music ]
    then
    wget http://files.freeswitch.org/$sounds_music
fi
tar zxvf $sounds_music
cd ..


cd freeswitch-sounds-en-us-callie
if [ ! -f $sounds_en_us_callie ]
    then
    wget http://files.freeswitch.org/$sounds_en_us_callie
fi
tar zxvf $sounds_en_us_callie
cd ..


cd freeswitch-sounds-ru-RU-elena
if [ ! -f $sounds_ru_RU_elena ]
    then
    wget http://files.freeswitch.org/$sounds_ru_RU_elena
fi
tar zxvf $sounds_ru_RU_elena
cd ..