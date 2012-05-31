#!/usr/bin/python

import getopt, time, sys

sys.path.insert( 0, r'/home/jb/project/libctb-0.16/python/module/linux') 

import ctb

def main():

    baudrate = 19200

    devname = ""

    protocol = '8N1'

    try:
        opt,arg = getopt.getopt(sys.argv[1:],
                                'b:d:p:',
                                ['baudrate=',
                                 'device=',
                                 'protocol='
                                 ])

    except getopt.GetoptError:
        print "usage: parity.py [options]\n"\
              "\t-b baudrate\n"\
              "\t--baudrate=baudrate"\
              "\t-d device\n"\
              "\t--device=serial device name like /dev/ttyS0 or COM1\n"\
              "\t-h\n"\
              "\t--help print this\n"\
              "\n"
        sys.exit(0)

    for o,a in opt:

        if o in ("-b","--baudrate"):

            baudrate = int(a)

        if o in ("-d","--device"):

            devname = a

        if o in ("-p","--protocol"):

            protocol = a


    print "Using ctb version " + ctb.GetVersion()

    dev = ctb.SerialPort()

    if dev.Open( devname, baudrate, protocol ) < 0:

        print "Cannot open " + devname +  "\n"

    # send the following string with a always set parity bit
    dev.SetParityBit( 1 )

    dev.Writev( "Hello World" )

    # send the following string with a always cleared parity bit
    dev.SetParityBit( 0 )

    dev.Writev( "Hello World" )

main()

