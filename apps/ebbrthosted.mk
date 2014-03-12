ifeq ($(strip ${EBBRT_SRCDIR}),)
 EBBRT_SRCDIR=${HOME}/EbbRT
 $(info EBBRT_SRCDIR not set defaulting to $(EBBRT_SRCDIR))
endif

ebbrt_hosted = ${EBBRT_SRCDIR}/hosted	
ebbrt_hostedinc = ${EBBRT_SRCDIR}/hosted/src/include
ebbrt_commoninc = ${EBBRT_SRCDIR}/common/src/include 

ifeq ($(strip ${EBBRT_BUILDTYPE}),Debug)
  OPTFLAGS ?= -O2 -g
else ifeq ($(strip ${EBBRT_BUILDTYPE}),Release)
  OPTFLAGS ?= -O2
else 
  $(error EBBRT_BUILDTYPE must be set to either Debug or Release)
endif

INCLUDES := \
	-I $(ebbrt_hostedinc) \
	-I $(ebbrt_commoninc) 

CXXFLAGS := -std=c++11 $(INCLUDES) $(OPTFLAGS)

ebbrt_libdir := lib
ebbrt_lib := ${ebbrt_libdir}/libEbbRT.a

# default is to have at least one baremetal image that matches 
# the name of our target
ifeq ($(strip ${EBBRT_BM_IMGS}),)
  EBBRT_BM_IMGS=$(abspath ../../../baremetal/build/$(EBBRT_BUILDTYPE)/$(target))
  $(info EBBRT_BM_IMGS not set defaulting to $(EBBRT_BM_IMGS))
endif

bm_imgdir = bm
bm_imgs = $(addprefix ${bm_imgdir}/,$(notdir $(EBBRT_BM_IMGS)))

$(info bm_imgdir: $(bm_imgdir) bm_imgs: $(bm_imgs))

app_objects := $(app_sources:.cc=.o)

${target}: $(app_objects) $(ebbrt_lib) $(bm_imgs)
	$(CXX) $(OPTFLAGS)  -Wl,--whole-archive $(app_objects) $(ebbrt_lib) -Wl,--no-whole-archive \
	-lboost_filesystem -lboost_system -lcapnp -lkj -lfdt -pthread -o $@
	@echo CREATED: $(abspath ${target})

${ebbrt_libdir}:
	mkdir ${ebbrt_libdir}

${ebbrt_libdir}/Makefile: ${ebbrt_libdir}
	(cd $(ebbrt_libdir); cmake -DCMAKE_BUILD_TYPE=${EBBRT_BUILDTYPE} ${ebbrt_hosted})

${ebbrt_lib}: ${ebbrt_libdir}/Makefile
	make -C ${ebbrt_libdir} 

${bm_imgdir}: 
	mkdir ${bm_imgdir}

${bm_imgs}: ${bm_imgdir} ${EBBRT_BM_IMGS}
	cp ${EBBRT_BM_IMGS}.elf* ${bm_imgdir}

${EBBRT_BM_IMGS}:
	make -C $(dir $@)

.PHONY: distclean

distclean: clean

clean:
	-$(RM) $(wildcard $(app_objects) $(target))
	-$(RM) -rf $(wildcard $(bm_imgdir))
	-$(RM) -rf $(wildcard $(ebbrt_libdir))

# Places to find source files assume that make is excuting in one of the
# build directories
# ../../src : hosted source for this application
# ../../../src: Common source for this application both baremetal and hosted
# ../../../../src: Common hosted and baremetal source files that are provide for all apps
# ../../../../src/hosted: Common hosted source files that are provided for all applications to use
VPATH := $(abspath ../../src):$(abspath ../../../src):$(abspath ../../../../src):$(abspath ../../../../src/hosted)
