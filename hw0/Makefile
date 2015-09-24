CXX=g++
INCLUDES=

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
FLAGS=-D__UNIX_JACK__ -c
LIBS=-lasound -lpthread -ljack -lstdc++ -lm
endif
ifeq ($(UNAME), Darwin)
FLAGS=-D__MACOSX_CORE__ -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon -lstdc++ -lm
endif


OBJS=   RtAudio.o sig-gen.o

sig-gen: $(OBJS)
	$(CXX) -std=c++11 -o sig-gen $(OBJS) $(LIBS)

sig-gen.o: sig-gen.cpp RtAudio.h
	$(CXX) -std=c++11 $(FLAGS) sig-gen.cpp

RtAudio.o: RtAudio.h RtAudio.cpp
	$(CXX) $(FLAGS) RtAudio.cpp

clean:
	rm -f *~ *# *.o sig-gen
