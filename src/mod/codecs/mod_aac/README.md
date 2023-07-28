# mod_aac

FreeSWITCH module for AAC Codec. https://en.wikipedia.org/wiki/FAAC

This mod was written many years ago when I learning to write FreeSWITCH modules. It probably doesn't build on the current version of FreeSWITCH.

The code is simple and hope still useful.

Pull Request is welcome.

## ToDo:

* [ ] Fix build on latest FreeSWITCH master
* [ ] Add cmake or autotools build scripts
* [ ] Add some test cases

## build

Put this mod in the freeswitch source dir, your freeswitch should already been built and installed.

```
cd freeswitch/src/mod/
mkdir rts
cd rts
git clone https://github.com/rts-cn/mod_aac
cd mod_aac
make
make install
```

## load

```
load mod_aac
```

## FAQ

Q: What License?

A: Same as FreeSWITCH.

Q: Do you accept Pull Request?

A: Sure. Thanks.
