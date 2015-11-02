TINYXMLSRCS = TinyXML/tinyxml.cpp TinyXML/tinyxmlerror.cpp TinyXML/tinyxmlparser.cpp

MELDDEBUGGER_SRCS = Debugger/debug_Simhandler.cpp Debugger/debug_Simprompt.cpp Debugger/types.cpp

MELDPROCESS_SRCS = $(MELDDEBUGGER_SRCS) meldProcessScheduler.cpp meldProcessVM.cpp meldProcessVMCommands.cpp meldProcessDebugger.cpp meldProcessEvents.cpp

MELDINTERPRET_SRCS = meldInterpretScheduler.cpp meldInterpretVM.cpp meldInterpretMessages.cpp meldInterpretEvents.cpp 

BASESIMULATOR_SRCS = $(MELDPROCESS_SRCS) $(MELDINTERPRET_SRCS) $(TINYXMLSRCS) simulator.cpp buildingBlock.cpp blockCode.cpp scheduler.cpp world.cpp network.cpp events.cpp glBlock.cpp interface.cpp openglViewer.cpp shaders.cpp vecteur.cpp matrice44.cpp color.cpp camera.cpp objLoader.cpp vertexArray.cpp trace.cpp clock.cpp configStat.cpp commandLine.cpp cppScheduler.cpp
BASESIMULATOR_OBJS = $(BASESIMULATOR_SRCS:.cpp=.o)
BASESIMULATOR_DEPS = $(BASESIMULATOR_SRCS:.cpp=.depends)

BLINKYBLOCKS_SRCS = $(DEBUGGER) blinkyBlocksSimulator.cpp blinkyBlocksBlock.cpp blinkyBlocksBlockCode.cpp blinkyBlocksScheduler.cpp blinkyBlocksWorld.cpp blinkyBlocksGlBlock.cpp
BLINKYBLOCKS_OBJS = $(BLINKYBLOCKS_SRCS:.cpp=.o) $(BASESIMULATOR_OBJS)
BLINKYBLOCKS_DEPS = $(BLINKYBLOCKS_SRCS:.cpp=.depends) $(BASESIMULATOR_DEPS)

SRCS = $(BASESIMULATOR_SRCS) $(BLINKYBLOCKS_SRCS) $(MELDPROCESS_SRCS)$(MELDINTERPRET_SRCS)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OUT = libsimBlinkyBlocks.a

INCLUDES = $(GLOBAL_INCLUDES) -I.

LIBS = $(GLOBAL_LIBS)

CCFLAGS = $(GLOBAL_CCFLAGS)

CC = g++


.PHONY: clean all

.cpp.o:
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

all: libsimBlinkyBlocks.a

libsimBlinkyBlocks.a: $(BASESIMULATOR_OBJS) $(BLINKYBLOCKS_OBJS)
	mkdir -p ../lib
	ar rcs libsimBlinkyBlocks.a $(BASESIMULATOR_OBJS) $(BLINKYBLOCKS_OBJS)
	cp libsimBlinkyBlocks.a ../lib

-include $(DEPS)

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS) ../lib/*.a ../include/*.h
	rm -rf ../lib

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

