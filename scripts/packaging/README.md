# Installing FreeSWITCH using FSGET

## Prerequisites

### Dependencies
Make sure you have `cURL` binary installed or use any other downloader.
To install curl type
```
apt update && apt install -y curl
```

### Authentication required

A `SignalWire Personal Access Token` (PAT) or an `API Token` (for enterprise customers) is required to access FreeSWITCH install packages.

[HOWTO Create a SignalWire Personal Access Token](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/Installation/how-to-create-a-personal-access-token/how-to-create-a-personal-access-token)

[Upgrade to FreeSWITCH Advantage](https://signalwire.com/products/freeswitch-enterprise)

## Configuring FreeSWITCH Debian repository
```
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token> [release|prerelease] [install]
```

Notice that `fsget` accepts arguments:
- `<PAT or FSA token>` (Required)
- `[release|prerelease]` (Optional) - `release` by default, `prerelease` is what you see in the `master` branch
- `[install]` (Optional) - If missing it will not install FreeSWITCH automatically after configuring the repository

`FreeSWITCH Community` or `FreeSWITCH Enterprise` version is installed based on the token provided.

Enterprise customers may install `FreeSWITCH Community` versions by using a `SignalWire Personal Access Token` instead of an `API Token`.
## Installing FreeSWITCH
If not installed already type

```
apt-get install -y freeswitch-meta-all
```

Enjoy using FreeSWITCH!