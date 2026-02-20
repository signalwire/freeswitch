# Coverity scan

## Build an image for Coverity
```
docker build -t coverity --build-arg REPOTOKEN=<signalwire token> --build-arg COVERITYTOKEN=<coverity token> .
```

## Scan FreeSWITCH using a Coverity image
```
docker run --rm -itv .:/data -e FSBRANCH="master" coverity
```

This will output `freeswitch.tgz` file to the current folder

## Uploading the result to the Coverity server
```
curl --form token=<coverity token> \
  --form email=andrey@signalwire.com \
  --form file=@freeswitch.tgz \
  --form version="Version" \
  --form description="Description" \
  https://scan.coverity.com/builds?project=FreeSWITCH
```