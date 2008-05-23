# The make files for this package exploit features of GNU Make that
# other Makes do not have.  Because it is a common mistake for users
# to try to build with a different Make, we have this make file that
# does nothing but tell the user to use GNU Make.

# If the user were using GNU Make now, this file would not get used because
# GNU Make uses a make file named "GNUmakefile" in preference to "Makefile"
# if it exists.  This package contains a "GNUmakefile".

default: all

all install clean dep depend:
	@echo "You must use GNU Make to build this.  You are running some "
	@echo "other Make.  GNU Make may be installed on your system with "
	@echo "the name 'gmake'.  If not, see http://www.gnu.org/software ."
	@echo
	false
