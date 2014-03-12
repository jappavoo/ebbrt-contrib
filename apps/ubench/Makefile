%::
	make -C hosted/build  $@
	make -C baremetal/build $@

.PHONY: all Release Debug

all: Release

Release:
	make -C hosted/build/Release  -j8
Debug:
	make -C hosted/build/Debug -j8
