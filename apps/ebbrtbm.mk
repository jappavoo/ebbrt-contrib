ifeq ($(strip ${EBBRT_SRCDIR}),)
 EBBRT_SRCDIR=${HOME}/EbbRT
 $(info EBBRT_SRCDIR not set defaulting to $(EBBRT_SRCDIR))
endif	
baremetal = ${EBBRT_SRCDIR}/baremetal

ifeq ($(strip ${EBBRT_OPTFLAGS}),)
 $(error You must set EBBRT_OPTFLAGS as you need. eg. -g3 -O4 -flto -DNDEBUG for an optimized build or -g3 -O0 for a non-optimized debug build)
endif	

optflags = ${EBBRT_OPTFLAGS}
include $(baremetal)/build.mk

VPATH += ../../src

.PRECIOUS: $(app_objects)
.PHONY: all distclean

$(info $(.PRECIOUS))

all: $(target).iso

distclean: clean
	-$(RM) -rf $(wildcard include ext sys src $(app_objects) $(target).*)
