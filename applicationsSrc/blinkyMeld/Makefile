SRCS = blinkyMeld.cpp blinkyMeldBlockCode.cpp

OUT = blinkyMeld

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

INCLUDES = -I. -I../../simulatorCore/src $(GLOBAL_INCLUDES) 

LIBS = $(GLOBAL_LIBS) -L../../simulatorCore/lib

SIMULATORLIB = ../../simulatorCore/lib/libsimBlinkyBlocks.a

CCFLAGS = $(GLOBAL_CCFLAGS)
CC = g++

.PHONY: clean all

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

all: $(OUT)

autoinstall: $(OUT)
	cp $(OUT) $(APPDIR)

$(APPDIR)/$(OUT): $(OUT)


$(OUT): $(SIMULATORLIB) $(OBJS) 
	$(CC) -o $(OUT) $(OBJS) $(LIBS)

-include $(DEPS)

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
