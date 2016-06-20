# This is a convience Makefile to constuct a fresh install
# of EbbRT both Release and Debug),its assocaited toolchain,
# and supporting libraries.
#
# To use it simple create a directory under which you would
# like your installs to live and copy or link this file there 
# as the Makefile.  Then run make.  This will produce two 
# subdirectories :  Release and Debug under which you will find
# seperate sysroot installs of EbbRT.  In particular you will find
# a bin directory under both that contains ebbrt gcc installs that
# will let you build native (backend binaries) bootable ebbrt binaries.

EBBRTREPO ?= https://github.com/dschatzberg/EbbRT.git -b toolchain
EBBRTDIR ?= EbbRT
EBBRTTOOLCHAINDIR = ${EBBRTDIR}/toolchain

.PHONY: all Debug Release clean distclean

all: Debug Release

Debug/.dirstamp:
	@mkdir Debug
	@touch $@

Release/.dirstamp:
	@mkdir Release
	@touch $@

${EBBRTDIR}/.dirstamp: 
	git clone --recursive ${EBBRTREPO} ${EBBRTDIR}
	@touch ${EBBRTDIR}/.dirstamp

Debug: Debug/.dirstamp ${EBBRTDIR}/.dirstamp
	make -j8 -C Debug -f ../${EBBRTTOOLCHAINDIR}/Makefile DEBUG=1

Release: Release/.dirstamp ${EBBRTDIR}/.dirstamp
	make -j8 -C Debug -f ../${EBBRTTOOLCHAINDIR}/Makefile

clean:
	-${RM} -r Debug Release 

distclean: clean
	-${RM} -r ${EBBRTDIR}
