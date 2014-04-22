EBBRT_APP_OBJECTS := ubench.o argv.o Unix.o CmdLineArgs.o Environment.o InputStreamCommon.o InputStream.o FS.o
EBBRT_TARGET := ubench

include $(abspath ../../../../ebbrtbaremetal.mk)
