# Building FreeSWITCH packages using `FSDEB`
## Prerequisites
FreeSWITCH packages can be built when FreeSWITCH is cloned using `git` only.  
(Methods described here won't work if you download a source tarball and extract it)

Please make sure you have `git` and `curl` installed:
```bash
apt-get update
apt-get install -y git curl
```

## Cloning FreeSWITCH
Assuming you build Debian packages for a FreeSWITCH release (this can be your fork or another branch as well).

```bash
cd /usr/src
git clone https://github.com/signalwire/freeswitch -b v1.10
```

## Configuring FreeSWITCH Debian repo (for dependencies)
Since we are building a FreeSWITCH release let's configure FreeSWITCH Community Release Debian repo.  
We recommend using [FSGET](/scripts/packaging).  

Replace `<PAT or API token>` with your `SignalWire Personal Access Token (PAT)`  
[HOWTO Create a SignalWire Personal Access Token](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/Installation/how-to-create-a-personal-access-token/how-to-create-a-personal-access-token)
```bash
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token>
```

## Building packages with `FSDEB`
```bash
curl -sSL https://freeswitch.org/fsdeb | bash -s -- -b 999 -o /usr/src/fsdebs/ -w /usr/src/freeswitch
```
That's pretty much it!

## Output
`FSDEB` will generate `.deb`, `.dsc`, `.changes`, and `.tar.*` files in the output directory:
```bash
ls -la /usr/src/fsdebs/
```

## Usage
You may be interested in other arguments of `FSDEB`:
```bash
curl -sSL https://freeswitch.org/fsdeb | bash -s -- -b BUILD_NUMBER -o OUTPUT_DIR [-w WORKING_DIR]
```

Required:
- `-b`: Build number (part of package version)
- `-o`: Output directory for packages

Optional:
- `-w`: Working directory (defaults to git root, needs to be git tree)
