#!/bin/sh
#
# spandsp fax tests
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

run_tsb85_test()
{
    rm -f fax_tests_1.tif
    echo ./tsb85_tests ${TEST}
    ./tsb85_tests -x ../spandsp/fax-tests.xml ${TEST} 2>xyzzy2
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo tsb85_tests ${TEST} failed!
        exit $RETVAL
    fi
}

for TEST in PPS-MPS-lost-PPS V17-12000-V29-9600 Phase-D-collision
do
    run_tsb85_test
done
