#! /usr/bin/python
# $Id: setupmingw32.py,v 1.1 2007/06/12 23:04:13 nanard Exp $
# the MiniUPnP Project (c) 2007 Thomas Bernard
# http://miniupnp.tuxfamily.org/ or http://miniupnp.free.fr/
#
# python script to build the miniupnpc module under unix
#
from distutils.core import setup, Extension
setup(name="miniupnpc", version="1.0-RC6",
      ext_modules=[
	         Extension(name="miniupnpc", sources=["miniupnpcmodule.c"],
	                   libraries=["ws2_32"],
			           extra_objects=["libminiupnpc.a"])
			 ])

