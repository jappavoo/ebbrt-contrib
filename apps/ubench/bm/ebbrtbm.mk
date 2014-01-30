ifeq ($(strip ${EBBRT_SRCDIR}),)
 $(error You must set EBBRT_SRCDIR to the path of an EbbRT source tree)
endif	
src = ${EBBRT_SRCDIR}
VPATH = ${EBBRT_SRCDIR}:../../src

ifeq ($(strip ${EBBRT_OPTFLAGS}),)
 $(error You must set EBBRT_OPTFLAGS as you need. eg. -g3 -O4 -flto -DNDEBUG for an optimized build or -g3 -O0 for a non-optimized debug build)
endif	

optflags = ${EBBRT_OPTFLAGS}
include ${EBBRT_SRCDIR}/build.mk

.PHONY: all distclean

all: $(target).iso

distclean: clean
	-$(RM) -rf $(wildcard ext sys $(target).*)
