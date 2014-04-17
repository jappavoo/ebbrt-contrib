%::
	$(MAKE) -C hosted/build  $@
	$(MAKE) -C baremetal/build $@

.PHONY: all Release Debug

all: Release

Release:
	$(MAKE) -C hosted/build/Release  -j8
Debug:
	$(MAKE) -C hosted/build/Debug -j8
