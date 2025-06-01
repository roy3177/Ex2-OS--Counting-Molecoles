.PHONY: all clean

SUBDIRS := part1-AtomicWarehouse part2-MoleculesRequest part3-Consol part4-options part5-Uds part6-Concurrency

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
