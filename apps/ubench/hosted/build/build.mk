app_sources := ubench.cc ubenchCommon.cc hoardThreadTest.cc Unix.cc CmdLineArgs.cc Environment.cc InputStreamCommon.cc InputStream.cc FS.cc
target := ubench

include $(abspath ../../../../ebbrthosted.mk)

ubenchCommon.o: ubenchCommon.cc
	${CXX} ${CXXFLAGS} -fno-devirtualize ../../../src/ubenchCommon.cc -c 
