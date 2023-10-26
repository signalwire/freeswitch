# Docker Setup
These are the official Docker files for master branch and the current release packages.

## Volumes
These containers are set up so that you can mount your freeswitch configuration from a host or data volume container.

To mount freeswitch Configuration
```
-v $(pwd)/configuration:/etc/freeswitch
```

To mount tmp directory for storing recordings, etc
```
-v $(pwd)/tmp:/tmp
```

The container also has a healthcheck where it does a fs_cli status check to make sure the freeswitch service is still running.

## Ports

The container should be run with host networking using `docker run --network host ...`.

If you prefer to (or for some reason must) publish individual ports via `--publish/-p`, refer to this [issue](https://github.com/moby/moby/issues/11185) and this [potential workaround](https://hub.docker.com/r/bettervoice/freeswitch-container/) regarding using docker with large port ranges.

The following ports will be used, depending upon your specific configuration:

- 5060/tcp, 5060/udp, 5080/tcp, 5080/udp - SIP signaling
- 5061/tcp, 5081/tcp - SIPS signaling
- 5066/tcp, 7443/tcp - WebSocket signaling
- 8021/tcp - the Event Socket
- 16384-32768/udp,  64535-65535/udp - media


If you wish to help improve these please submit a pull request at:

https://github.com/signalwire/freeswitch

Thanks,
/b
