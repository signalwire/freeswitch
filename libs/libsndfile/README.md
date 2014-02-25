# libsndfile

libsndfile is a C library for reading and writing files containing sampled audio
data.

## Hacking

The canonical source code repository for libsndfile is at
[https://github.com/erikd/libsndfile/][github].

You can grab the source code using:

    $ git clone git://github.com/erikd/libsndfile.git

Building on Linux, OSX and Windows (Using GNU GCC) will require a number of GNU
and other Free and Open Source Software tools including:

* [Autoconf][autoconf]
* [Autogen][autogen]
* [Automake][automake]
* [Libtool][libtool]
* [Pkgconfig][pkgconfig]
* [Python][python]

If you are on Linux, its probably best to install these via your Linux
distribution's package manager.

If you want to compile libsndfile with support for formats like FLAC and
Ogg/Vorbis you will also need to install the following optional libraries:

* [FLAC][flac]
* [libogg][libogg]
* [libvorbis][libvorbis]

Support for these extra libraries is an all or nothing affair. Unless all of
them are installed none of them will be supported.

    $ ./autogen.sh

Running `autogen.sh` also installs a git pre-commit hook. The pre-commit hook
is run each time a user tries to commit and checks code about to be committed
against coding guidelines.

Nest step is to run configure, with the following configure options being
recommended for anyone contemplating sending libsndfile patches:

    $ ./configure --enable-gcc-werror

Finally libsndfile can be built and tested:

    $ make
    $ make check

## Submitting Patches.

* Patches should pass all pre-commit hook tests.
* Patches should always be submitted via a either Github "pull request" or a
  via emailed patches created using "git format-patch".
* Patches for new features should include tests and documentation.
* Patches to fix bugs should either pass all tests, or modify the tests in some
  sane way.
* When a new feature is added for a particular file format and that feature
  makes sense for other formats, then it should also be implemented for one
  or two of the other formats.





[autoconf]: http://www.gnu.org/s/autoconf/
[autogen]: http://www.gnu.org/s/autogen/
[automake]: http://www.gnu.org/software/automake/
[flac]: http://flac.sourceforge.net/
[github]: https://github.com/erikd/libsndfile/
[libogg]: http://xiph.org/ogg/
[libtool]: http://www.gnu.org/software/libtool/
[libvorbis]: http://www.vorbis.com/
[pkgconfig]: http://www.freedesktop.org/wiki/Software/pkg-config
[python]: http://www.python.org/
