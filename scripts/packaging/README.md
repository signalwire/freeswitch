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
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token> [release|prerelease] [install|source|build-dep|showsrc]
```

Notice that `fsget` accepts arguments:
- `<PAT or FSA token>` (Optional) - omit if the FreeSWITCH apt repository is already configured (the credentials are already in `/etc/apt/auth.conf`)
- `[release|prerelease]` (Optional) - `release` by default, `prerelease` is what you see in the `master` branch
- `[install|source|build-dep|showsrc]` (Optional) - action to perform after the repository is configured. If missing it will not perform any action after configuring the repository.

`FreeSWITCH Community` or `FreeSWITCH Enterprise` version is installed based on the token provided.

Enterprise customers may install `FreeSWITCH Community` versions by using a `SignalWire Personal Access Token` instead of an `API Token`.

Arguments can be supplied in any order — `fsget` recognises them by pattern.

### Available actions

| Action | Description |
| --- | --- |
| `install` | Install `freeswitch-meta-all` after the repository is configured. |
| `source` | Download the FreeSWITCH source package for the current build architecture. |
| `build-dep` | Install the FreeSWITCH build dependencies for the current build architecture. |
| `showsrc` | Print the FreeSWITCH source version for the current build architecture and exit. |

Because FreeSWITCH source packages encode the build architecture in the version string, `source` and `build-dep` look up the exact version for the current architecture before invoking `apt-get` — this prevents `apt` from picking a candidate built for a different architecture.

### Examples

Configure the repository and install FreeSWITCH:
```
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token> release install
```

Configure the prerelease repository and install build dependencies:
```
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token> prerelease build-dep
```

Token already configured — just install build dependencies for the current architecture:
```
curl -sSL https://freeswitch.org/fsget | bash -s build-dep
```

Token already configured — fetch the source package for the current architecture:
```
curl -sSL https://freeswitch.org/fsget | bash -s source
```

Print the FreeSWITCH source version for the current architecture:
```
curl -sSL https://freeswitch.org/fsget | bash -s showsrc
```

## Installing FreeSWITCH
If not installed already type

```
apt-get install -y freeswitch-meta-all
```

Enjoy using FreeSWITCH!