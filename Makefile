PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

OBJS = wwgsl.o
LIBS = -lcurl

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g
else ifeq ($(BUILD_MODE),run)
	CFLAGS += -O2
else ifeq ($(BUILD_MODE),linuxtools)
	CFLAGS += -g -pg -fprofile-arcs -ftest-coverage
	LDFLAGS += -pg -fprofile-arcs -ftest-coverage
	EXTRA_CLEAN += wwgsl.gcda wwgsl.gcno $(PROJECT_ROOT)gmon.out
	EXTRA_CMDS = rm -rf wwgsl.gcda
else
    $(error Build mode $(BUILD_MODE) not supported by this Makefile)
endif

all:	wwgsl

wwgsl:	$(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(EXTRA_CMDS)

%.o:	$(PROJECT_ROOT)%.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

%.o:	$(PROJECT_ROOT)%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -fr wwgsl $(OBJS) $(EXTRA_CLEAN)
