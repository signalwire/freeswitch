#!/usr/bin/python

import getopt, time, sys

sys.path.insert( 0, r'/home/jb/project/libctb-0.16/python/module/linux') 

import ctb

def main():

    devname = ""

    try:
        opt,arg = getopt.getopt(sys.argv[1:],
                                'd:',
                                ['device='
                                 ])

    except getopt.GetoptError:
        print "usage: parity.py [options]\n"\
              "\t-d device\n"\
              "\t--device=serial device name like /dev/ttyS0 or COM1\n"\
              "\t-h\n"\
              "\t--help print this\n"\
              "\n"
        sys.exit(0)

    for o,a in opt:

        if o in ("-d","--device"):

            devname = a

    print "Using ctb version " + ctb.GetVersion()

    dev = ctb.SerialPort()

    if dev.Open( devname, 38400 ) < 0:

        print "Cannot open " + devname +  "\n"

    dev.SetLineState( ctb.DTR )

    dev.ClrLineState( ctb.RTS )

    for i in range( 0, 100 ) :
        
        time.sleep( 0.01 )

        dev.ChangeLineState( ctb.DTR | ctb.RTS )

main()

