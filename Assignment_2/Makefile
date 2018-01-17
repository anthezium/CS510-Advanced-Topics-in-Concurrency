# TUNABLE PARAMETERS
#DEBUG           = TRUE
PRINT_WORKER    = 0
PRINT_PARENT    = 1
PRINT_VERBOSE   = 0
PRINT_CSV       = 1

WARN            = -Wno-attributes -Wno-format-extra-args -Wno-implicit-function-declaration
CPUS            = $(shell grep 'processor[[:space:]]*:' /proc/cpuinfo | wc -l)
JOBS            = $(shell echo "$(CPUS) - 1" | bc)
MAKEFLAGS      += "-j $(JOBS)"
CLEAN_COMMENTS  = grep -v '^\# [[:digit:]]\+ '
INSERT_NEWLINES = sed 's/__N3WLN__/\n/g'
SKIP_NEWLINES   = sed 's/__N3WLN__//g'
REINDENT        = echo -e "G=gg\n:wq\n" | vim 
CCOPTS1         = -pthread -I/u/theod/builds/include -DPRINT_WORKER=$(PRINT_WORKER) -DPRINT_PARENT=$(PRINT_PARENT) -DPRINT_VERBOSE=$(PRINT_VERBOSE) -DPRINT_CSV=$(PRINT_CSV)
CCOPTS2         = -L/u/theod/builds/lib -lurcu-qsbr 

ifeq ($(DEBUG),TRUE)
  CC1           = nice gcc -E $(CCOPTS1) $(WARN)
  CC2           = nice gcc -g $(CCOPTS1) $(WARN)
else
  CC            = nice gcc $(CCOPTS1) $(WARN) -D__N3WLN__=
endif

OBJS            = util.o worker.o tests.o

# make makes the first goal the default, you can override this with
#.DEFAULT_GOAL := all
all: test

util: util.h util.c
ifeq ($(DEBUG),TRUE)
	$(CC1) $@.c | $(CLEAN_COMMENTS) | $(INSERT_NEWLINES) > $@CPP.c 
	#$(REINDENT) $@CPP.c
	$(CC2) -xc -c -o $@.o $@CPP.c
else
	$(CC) $@.c -c -o $@.o
endif

worker: worker.c worker.h tests.h util
ifeq ($(DEBUG),TRUE)
	$(CC1) $@.c | $(CLEAN_COMMENTS) | $(INSERT_NEWLINES) > $@CPP.c 
	#$(REINDENT) $@CPP.c
	$(CC2) -xc -c -o $@.o $@CPP.c
else
	$(CC) $@.c -c -o $@.o
endif

tests: util tests.c tests.h
ifeq ($(DEBUG),TRUE)
	$(CC1) $@.c | $(CLEAN_COMMENTS) | $(INSERT_NEWLINES) > $@CPP.c 
	#$(REINDENT) $@CPP.c
	$(CC2) -xc -c -o $@.o $@CPP.c
else
	$(CC) $@.c -c -o $@.o
endif

test: main.c util worker tests
ifeq ($(DEBUG),TRUE)
	$(CC1) main.c | $(CLEAN_COMMENTS) | $(INSERT_NEWLINES) > mainCPP.c
	#$(REINDENT) $@CPP.c
	$(CC2) -o test mainCPP.c $(OBJS) $(CCOPTS2)
else
	$(CC) -o test main.c $(OBJS) $(CCOPTS2)
endif

.PHONY: clean
clean: 
	rm -f $(OBJS) $(wildcard *CPP.c) test
