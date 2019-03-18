.PHONY: all win clean tool
.DEFAULT_GOAL := hash_extr

CC := gcc
CXX := g++

CC_WIN               := i686-w64-mingw32-gcc
CXX_WIN               := i686-w64-mingw32-g++


program_NAME := hash_extr
program_WIN_NAME := hash_extr.exe
OBJS_C_ALL = hccvt common
program_C_SRCS := $(foreach OBJ, $(OBJS_C_ALL),src/$(OBJ).c)
OBJS_CXX_ALL = inicfg
program_CXX_SRCS := $(foreach OBJ, $(OBJS_CXX_ALL),src/$(OBJ).cpp)

program_C_OBJS := ${program_C_SRCS:.c=.o}
program_CXX_OBJS := ${program_CXX_SRCS:.cpp=.o}
program_OBJS := $(program_C_OBJS) $(program_CXX_OBJS)
program_INCLUDE_DIRS := include
program_LIBRARY_DIRS := /usr/local/bin
program_LIBRARIES :=

CFLAGS = -std=gnu99
CXXFLAGS = -std=gnu++11
CPPFLAGS += $(foreach includedir,$(program_INCLUDE_DIRS),-I$(includedir))
LDFLAGS += $(foreach librarydir,$(program_LIBRARY_DIRS),-L$(librarydir))
LDFLAGS += $(foreach library,$(program_LIBRARIES),-l$(library))

$(program_NAME): hash_extr.cpp $(program_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) -I$(program_INCLUDE_DIRS)

clean:
	@- $(RM) $(program_NBME)
	@- $(RM) $(program_OBJS)


program_C_WIN_OBJS := ${program_C_SRCS:.c=.WIN.o}
program_CXX_WIN_OBJS := ${program_CXX_SRCS:.cpp=.WIN.o}
program_WIN_OBJS := $(program_C_WIN_OBJS) $(program_CXX_WIN_OBJS)

src/%.WIN.o:   src/%.c
	$(CC_WIN)   $(CFLAGS)   -c -o $@ $< -I$(program_INCLUDE_DIRS)

src/%.WIN.o:   src/%.cpp
	$(CXX_WIN)   $(CXXFLAGS)   -c -o $@ $< -I$(program_INCLUDE_DIRS)

$(program_WIN_NAME): hash_extr.cpp $(program_WIN_OBJS)
	$(CXX_WIN) -static-libgcc -static-libstdc++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -I$(program_INCLUDE_DIRS)
