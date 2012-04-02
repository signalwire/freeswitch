#!/usr/bin/python

import getopt, time, sys

sys.path.insert( 0, r'/home/jb/project/libctb-0.16/python/module/linux') 

import ctb

def DataBlock():
    data = ''
    for c in range( 0, 256):
        data += '%c' % c

    return data

def main():

    baudrate = 19200

    devname = ""

    try:
        opt,arg = getopt.getopt(sys.argv[1:],
                                'b:d:',
                                ['baudrate=',
                                 'device='
                                 ])

    except getopt.GetoptError:
        print "usage: protocol.py [options]\n"\
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

    print "Using ctb version " + ctb.GetVersion()

    dev = ctb.SerialPort()

    protocols = [
        '8N1','8O1','8E1','8S1','8M1'
        ]

    dev.SetTimeout( 1000 )

    for protocol in protocols:

        if dev.Open( devname, baudrate, protocol ) < 0:

            print "Cannot open " + devname +  "\n"

            sys.exit( 1 )

        else:

            print( "%i %s" % ( baudrate, protocol ) )

            for i in range(0, 4 ):

                 dev.Writev( "\x33" )

                 time.sleep( 0.0006 )

                 dev.Writev( "\x31" )

                 time.sleep( 0.0006 )

            time.sleep( 0.5 )
            
            dev.Close()


main()

