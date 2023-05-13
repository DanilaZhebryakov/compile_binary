CC := g++
CFLAGS := -D _DEBUG -g -ggdb3 -std=c++17 -O0 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Werror=return-type -Werror=format-extra-args -I ./
LFLAGS :=
OBJDIR := obj


OBJ_FUNC = $(addprefix $(OBJDIR)/, $(1:.cpp=.o))
DEP_FUNC = $(addprefix $(OBJDIR)/, $(1:.cpp=.d))

LIBDIR  := lib
LIBSRC  := $(wildcard $(LIBDIR)/*.cpp)
LIB_OBJ := $(call OBJ_FUNC, $(LIBSRC))


COMMON_SRC := $(wildcard compiler_out_lang/*.cpp)

TREE_SRC := $(wildcard tree/*.cpp) $(wildcard expr/*.cpp) $(wildcard program/*.cpp)
TREE_RUN_FILE := run/Tree

X86_64_SRC := $(wildcard toasm_x86_64/*.cpp)

.PHONY: all run clean libs tree

all: libs tree

clean:
	rm -r $(OBJDIR)
	rm    $(RUNFILE)
	
libs: $(LIB_OBJ)


tree: $(TREE_RUN_FILE)


run: 
	$(TREE_RUN_FILE)

OBJECTS_X86_64 := $(call OBJ_FUNC, $(X86_64_SRC))

OBJECTST := $(call OBJ_FUNC, $(COMMON_SRC) $(TREE_SRC))
$(TREE_RUN_FILE): libs $(OBJECTST) $(OBJECTS_X86_64)
	$(CC) $(LIB_OBJ) $(OBJECTST) $(OBJECTS_X86_64) -o $@ $(LFLAGS)


$(OBJDIR)/%.d : %.cpp
	mkdir -p $(dir $@)
	$(CC) $< -c -MMD -MP -o $(@:.d=.o) $(CFLAGS)

$(OBJDIR)/%.o : %.cpp
	mkdir -p $(dir $@)
	$(CC) $< -c -MMD -MP -o $@ $(CFLAGS)

-include $(wildcard $(OBJDIR)/*.d)