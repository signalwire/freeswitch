#!/bin/bash
aclocal -I ./config/
autoheader
autoconf
automake
