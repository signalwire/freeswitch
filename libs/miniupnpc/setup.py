#! /usr/bin/python
# $Id: setup.py,v 1.3 2009/04/17 20:59:42 nanard Exp $
# the MiniUPnP Project (c) 2007 Thomas Bernard
# http://miniupnp.tuxfamily.org/ or http://miniupnp.free.fr/
#
# python script to build the miniupnpc module under unix
#
# replace libminiupnpc.a by libminiupnpc.so for shared library usage
from distutils.core import setup, Extension
setup(name="miniupnpc", version="1.3",
      ext_modules=[
	         Extension(name="miniupnpc", sources=["miniupnpcmodule.c"],
			           extra_objects=["libminiupnpc.a"])
			 ])

