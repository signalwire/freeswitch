#!/bin/sh
#
# SpanDSP - a series of DSP components for telephony
#
# unpack_gsm0610_data.sh
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 2.1,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# The ETSI distribution file extracts to 5 ZIP files, called DISK1.ZIP to DISK5.ZIP
# These were originally the contents of 5 floppy disks. Disks 1 to 3 contain data
# files. However, disks 4 and 5 contain .EXE files, which unpack.... but only in an
# MS environment. These files need to be executed in a Windows or DOS environment,
# or a good emulation like FreeDOS or Wine.

ETSIDATA="../../../en_300961v080101p0.zip"

cd test-data
if [ -d etsi ]
then
    cd etsi
else
    mkdir etsi
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/etsi!
        exit $RETVAL
    fi
    cd etsi
fi
if [ -d gsm0610 ]
then
    cd gsm0610
else
    mkdir gsm0610
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/etsi/gsm0610!
        exit $RETVAL
    fi
    cd gsm0610
fi

if [ $1x ==  --no-exe-runx ]
then
    # Run the .exe files, which should be here
    ./FR_A.EXE
    ./FR_HOM_A.EXE
    ./FR_SYN_A.EXE
    ./FR_U.EXE
    ./FR_HOM_U.EXE
    ./FR_SYN_U.EXE
    exit 0
fi

# Clear out any leftovers from the past
rm -rf ASN.1.txt
rm -rf DISK1.ZIP
rm -rf DISK2.ZIP
rm -rf DISK3.ZIP
rm -rf DISK4.ZIP
rm -rf DISK5.ZIP
rm -rf *.EXE
rm -rf READ_FRA.TXT
rm -rf ACTION
rm -rf unpacked

if [ $1x ==  --no-exex ]
then
    # We need to prepare the .exe files to be run separately
    rm -rf *.INP
    rm -rf *.COD
    rm -rf *.OUT

    unzip ${ETSIDATA} >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot unpack the ETSI test vectors for GSM 06.10!
        exit $RETVAL
    fi
    unzip ./DISK4.ZIP >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot unpack the ETSI test vectors for GSM 06.10!
        exit $RETVAL
    fi
    unzip ./DISK5.ZIP >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot unpack the ETSI test vectors for GSM 06.10!
        exit $RETVAL
    fi
    rm -rf ASN.1.txt
    rm -rf DISK1.ZIP
    rm -rf DISK2.ZIP
    rm -rf DISK3.ZIP
    rm -rf DISK4.ZIP
    rm -rf DISK5.ZIP
    rm -rf READ_FRA.TXT

    # An environment which is emulating an MS one will probably need
    # to make the .EXE files actually executable.
    chmod 755 *.EXE

    echo "Now copy the files from the test-data/etsi/gsm0610 directory to a Windows,"
    echo "DOS or other machine which can run .exe files. Run each of the .exe"
    echo "files (there are 6 of them), and copy the whole directory back here."
    echo "You can then complete the creation of the working data directories"
    echo "with the command:"
    echo $0 "--no-exe-continue"
    exit 0
fi

unzip ${ETSIDATA} >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ETSI test vectors for GSM 06.10!
    exit $RETVAL
fi
#rm ${ETSIDATA}

rm -rf ASN.1.txt

if [ $1x !=  --no-exe-continuex ]
then
    # We need to extract and run the .exe files right now. For this to succeed
    # we must be running in an environment which can run .exe files. This has been
    # tested with Cygwin on a Windows XP machine.
    rm -rf *.INP
    rm -rf *.COD
    rm -rf *.OUT

    unzip ./DISK4.ZIP >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot unpack the ETSI test vectors for GSM 06.10!
        exit $RETVAL
    fi
    unzip ./DISK5.ZIP >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot unpack the ETSI test vectors for GSM 06.10!
        exit $RETVAL
    fi
    # An environment which is emulating an MS one will probably need
    # to make the .EXE files actually executable.
    chmod 755 *.EXE
    ./FR_HOM_A.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi
    ./FR_SYN_A.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi
    ./FR_A.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi
    ./FR_HOM_U.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi
    ./FR_SYN_U.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi
    ./FR_U.EXE >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot run ./FR_HOM_A.EXE
        exit $RETVAL
    fi

    rm -rf READ_FRA.TXT
fi

rm -rf DISK4.ZIP
rm -rf DISK5.ZIP
rm -rf *.EXE

chmod 644 *.INP
chmod 644 *.OUT
chmod 644 *.COD

# Create the directories where we want to put the test data files.
mkdir unpacked
mkdir unpacked/fr_A
mkdir unpacked/fr_L
mkdir unpacked/fr_U
mkdir unpacked/fr_homing_A
mkdir unpacked/fr_homing_L
mkdir unpacked/fr_homing_U
mkdir unpacked/fr_sync_A
mkdir unpacked/fr_sync_L
mkdir unpacked/fr_sync_U

# Disks 1, 2 and 3 simply unzip, and the files have sensible file names. We
# just need to rearrange the directories in which they are located.
unzip ./DISK1.ZIP >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ETSI test vectors for GSM 06.10!
    exit $RETVAL
fi
unzip ./DISK2.ZIP >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ETSI test vectors for GSM 06.10!
    exit $RETVAL
fi
unzip ./DISK3.ZIP >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ETSI test vectors for GSM 06.10!
    exit $RETVAL
fi
rm -rf DISK1.ZIP
rm -rf DISK2.ZIP
rm -rf DISK3.ZIP

mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK1/"* ./unpacked/fr_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/"Sync*.cod ./unpacked/fr_sync_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK2/"Seq*h.* ./unpacked/fr_homing_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/"Seq*h.* ./unpacked/fr_homing_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/Bitsync.inp" ./unpacked/fr_sync_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/Seqsync.inp" ./unpacked/fr_sync_L

mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/Homing01.cod" ./unpacked/fr_homing_L
mv "./ACTION/SMG#23/HOLD/0610_5~1/TESTSE~1/DISK3/Homing01.out" ./unpacked/fr_homing_L

rm -rf ACTION

# The files extracted by the .EXE files have messy naming, and are not in
# a sane directory layout. We rename and move them, to make the final result of
# the files extracted from all five of the original .ZIP files reasonably
# consistent, and easy to follow.
rm -rf READ_FRA.TXT

for I in SYN*_A.COD ;
do
    mv $I `echo $I | sed -e "s|SYN|./unpacked/fr_sync_A/Sync|" | sed -e "s/COD/cod/"`
done

for I in SYN*_U.COD ;
do
    mv $I `echo $I | sed -e "s|SYN|./unpacked/fr_sync_U/Sync|" | sed -e "s/COD/cod/"`
done

for I in SEQ*H_A.COD ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_A/Seq|" | sed -e "s/COD/cod/"`
done

for I in SEQ*H_U.COD ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_U/Seq|" | sed -e "s/COD/cod/"`
done

for I in SEQ*H_A.INP ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_A/Seq|" | sed -e "s/INP/inp/"`
done

for I in SEQ*H_U.INP ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_U/Seq|" | sed -e "s/INP/inp/"`
done

for I in SEQ*H_A.OUT ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_A/Seq|" | sed -e "s/OUT/out/"`
done

for I in SEQ*H_U.OUT ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_homing_U/Seq|" | sed -e "s/OUT/out/"`
done

for I in SEQ*-A.COD ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_A/Seq|" | sed -e "s/COD/cod/"`
done

for I in SEQ*-U.COD ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_U/Seq|" | sed -e "s/COD/cod/"`
done

for I in SEQ*-A.INP ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_A/Seq|" | sed -e "s/INP/inp/"`
done

for I in SEQ*-U.INP ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_U/Seq|" | sed -e "s/INP/inp/"`
done

for I in SEQ*-A.OUT ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_A/Seq|" | sed -e "s/OUT/out/"`
done

for I in SEQ*-U.OUT ;
do
    mv $I `echo $I | sed -e "s|SEQ|./unpacked/fr_U/Seq|" | sed -e "s/OUT/out/"`
done

mv HOM01_A.OUT ./unpacked/fr_homing_A/Homing01_A.out
mv HOM01_U.OUT ./unpacked/fr_homing_U/Homing01_U.out
mv SEQSYN_A.INP ./unpacked/fr_sync_A/Seqsync_A.inp
mv SEQSYN_U.INP ./unpacked/fr_sync_U/Seqsync_U.inp

echo "The ETSI test vectors for GSM 06.10 should now be correctly laid out in the"
echo "gsm0610 directory"
