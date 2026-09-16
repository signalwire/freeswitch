About
-----

This is an updated, minimized, official FreeSwitch docker image.
Container designed to run on host network.
Size of image decreased to 120MB (54MB compressed)
Significantly increased security:
1) removed all libs except libc, busybox, erlang, ca-certificates, gnupg2, passwd, curl, freeswitch and dependent libs.
2) removed 'system' API command from vanila config
3) updated FreeSwitch default SIP password to random value

Used environment variables
--------------------------

1) ```SOUND_RATES``` - rates of sound files that must be downloaded and installed. Available values ```8000```, ```16000```, ```32000```, ```48000```. May defined multiply values using semicolon as delimiter. Example ```SOUND_RATES=8000:16000```;
2) ```SOUND_TYPES``` - types of sound files that must be downloaded and installed. Available values music, ```en-us-callie```, ```en-us-allison```, ```ru-RU-elena```, ```en-ca-june```, ```fr-ca-june```, ```pt-BR-karina```, ```sv-se-jakob```, ```zh-cn-sinmei```, ```zh-hk-sinmei```. Example ```SOUND_TYPES=music:en-us-callie```;
3) ```EPMD``` - start epmd daemon, useful when you use mod_erlang and mod_kazoo FreeSwitch modules. Available values ```true```, ```false```.

Usage container
---------------

1) Creating volume for sound files. This may be skipped if you not use freeswitch MOH and other sound files.
```sh
docker volume create --name freeswitch-sounds 
```

2) Stating container
```sh
docker run --net=host --name freeswitch \
           -e SOUND_RATES=8000:16000 \
           -e SOUND_TYPES=music:en-us-callie \
           -v freeswitch-sounds:/usr/share/freeswitch/sounds \
           -v /etc/freeswitch/:/etc/freeswitch \
           dheaps/freeswitch
```

systemd unit file
-----------------
You can use this systemd unit file on your hosts.
```sh
$ cat /etc/systemd/system/freeswitch-docker.service
[Unit]
Description=freeswitch Container
After=docker.service network-online.target
Requires=docker.service


[Service]
Restart=always
TimeoutStartSec=0
#One ExecStart/ExecStop line to prevent hitting bugs in certain systemd versions
ExecStart=/bin/sh -c 'docker rm -f freeswitch; \
          docker run -t --net=host --name freeswitch \
                 -e SOUND_RATES=8000:16000 \
                 -e SOUND_TYPES=music:en-us-callie \
                 -v freeswitch-sounds:/usr/share/freeswitch/sounds \
                 -v /etc/kazoo/freeswitch/:/etc/freeswitch \
                 dheaps/freeswitch'
ExecStop=-/bin/sh -c '/usr/bin/docker stop freeswitch; \
          /usr/bin/docker rm -f freeswitch;'

[Install]
WantedBy=multi-user.target
```
Unit file can be placed to ```/etc/systemd/system/freeswitch-docker.service``` and enabled by command
```sh
systemd start freeswitch-docker.service
systemd enable freeswitch-docker.service
```

.bashrc file
------------
To simplify freeswitch management you can add alias for ```fs_cli``` to ```.bashrc``` file as example bellow.
```sh
alias fs_cli='docker exec -i -t freeswitch /usr/bin/fs_cli'
```

How to create custom container
------------------------------
This container created from scratch image by addiding required freeswitch files packaged to tar.gz archive.
To create custom container:

1. clone freeswitch repo
```sh
git clone https://github.com/signalwire/freeswitch.git
```
2. modify ```freeswitch/docker/master-min/Dockerfile``` with customizations  
 - Stage files are not inlcuded by default, but is the place to add additional packages/dependancies  
3. modify ```freeswitch/docker/master-min/make_root_fs.sh```  with customizations  
 - If files/packages were added to the stage image add them here
 - Additional installed packages should be added to the PACKAGES variable in fs_files_debian()
 - Additinoal installed files should be added in make_new_root()  
4. build custom container
```sh
docker build -t freeswitch_custom .
```

Read more
---------

[Dockerfile of older official FreeSwitch image](https://github.com/signalwire/freeswitch/tree/master/docker/release)
[Dockerfile of the updated FreeSwitch image that this image is based on](https://github.com/signalwire/freeswitch/tree/master/docker/master)
[Dockerfile of minimized base image FreeSwitch image that this image is based on](https://github.com/signalwire/freeswitch/tree/master/docker/base_image)
