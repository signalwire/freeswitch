# Docker Setup
These are the official Docker files for master branch and the current release packages.

## Volumes
These containers are setup so that you can mount your freeswitch configuration form a host or data volume container.

To mount freeswitch Configuration
```
-v $(pwd)/configuration:/etc/freeswitch
```

To mount tmp directory for storing recordings, etc
```
-v $(pwd)/tmp:/tmp
```

The container also has a healthcheck where it does a fs_cli status check to make sure the freeswitch service is still running.

# Ports

The container exposes the following ports:

- 5060/tcp 5060/udp 5080/tcp 5080/udp as SIP Signaling ports.
- 5066/tcp 7443/tcp as WebSocket Signaling ports.
- 8021/tcp as Event Socket port.
- 64535-65535/udp as media ports.
- 16384-32768/udp




If you wish to help improve these please submit a pull request at:

https://github.com/signalwire/freeswitch

Thanks,
/b
