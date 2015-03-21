EBBRT_BUILDTYPE=Release

include $(abspath ../build.mk)


ubenchCommon.o: ubenchCommon.cc
	${EBBRT_CXX} ${EBBRT_CPPFLAGS} $(EBBRT_CXXFLAGS) -fno-devirtualize ../../../src/ubenchCommon.cc -c 
