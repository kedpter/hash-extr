.PHONY: all win clean tool
.DEFAULT_GOAL := all

CC := gcc
CXX := g++

CC_WIN               := i686-w64-mingw32-g++


program_NAME := hash_extr
OBJS_C_ALL = hccvt common
program_C_SRCS := $(foreach OBJ, $(OBJS_C_ALL),src/$(OBJ).c)
OBJS_CXX_ALL = inicfg
program_CXX_SRCS := $(foreach OBJ, $(OBJS_CXX_ALL),src/$(OBJ).cpp) hash_extr.cpp
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

$(program_NAME): $(program_OBJS)
	$(LINK.cc) $(program_OBJS) -o $(program_NAME) $(LDFLAGS)

all: $(program_NAME)

clean:
	@- $(RM) $(program_NBME)
	@- $(RM) $(program_OBJS)

win:
	$(CC_WIN) $(CXXFLAGS) $(program_C_SRCS) $(program_CXX_SRCS) -o $(program_NAME).exe $(LDFLAGS) -I $(program_INCLUDE_DIRS)
