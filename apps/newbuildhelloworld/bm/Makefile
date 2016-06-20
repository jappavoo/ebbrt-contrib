# TO USE: $ PATH=$PATH:/home/jappavoo/Work/BuildSystem/EbbRT/toolchain/sysroot/usr/bin/ make 
CXX := x86_64-pc-ebbrt-g++
CXXFLAGS := -std=gnu++14
src := Printer.cc helloworld.cc
objs := $(src:.cc=.o)
target := helloworld.elf
.PHONY: clean all

# stuff to remove
#LIBS := -lebbrt 
#LIBS += -lcapnp -lkj -lacpica -ltbb 
#LIBS += -lstdc++ -lsupc++ -lgcc -lc 
#LIBS += -lg
#LIBS += -lnosys 
#LIBS += -lebbrt

#LDFLAGS := -nostartfiles
LDFLAGS += -T /home/jappavoo/Work/BuildSystem/EbbRT/baremetal/src/ebbrt.ld 
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000

all: $(target)

$(target): $(objs)
	  ${CXX} -v $(objs) ${LIBS} ${LDFLAGS} -o helloworld.elf

clean:
	${RM} $(wildcard $(objs) helloworld.elf)
