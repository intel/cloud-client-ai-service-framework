LIBRARY = libccai_log.so
SOURCE = ccai_log.c log.c
OBJECT = $(SOURCE:.c=.o)

TESTSRC = test.c
TESTBIN = tlog

CFLAGS = -fpic -Wall -fvisibility=hidden
ifdef NDEBUG
    CFLAGS += -O2
else
    CFLAGS += -g -O0
endif

all: $(LIBRARY)
test: $(TESTBIN)

clean:
	rm -f $(LIBRARY) $(OBJECT) $(TESTBIN)

$(LIBRARY): $(OBJECT)
	$(CC) -shared -o $@ $^

$(TESTBIN): $(TESTSRC) $(LIBRARY)
	$(CC) -g -O0 -o $(TESTBIN) $(TESTSRC) ./$(LIBRARY) -Wl,--dynamic-list=dynamic.list

.PHONY: all test
