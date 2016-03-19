C_FILES := $(shell find src -name '*.c')
C_FILES += $(shell find ../core/src -name '*.c')
OBJ_FILES := $(addprefix obj/,$(notdir $(C_FILES:.c=.o)))
LD_FLAGS := -g -lpthread -lm 
CC_FLAGS := -g -Wno-unused-value -Werror -O0 -D DEBUG
#COMPILER = gcc -std=c99 -pg
COMPILER = clang-3.5

main: $(OBJ_FILES)
	$(COMPILER) $(LD_FLAGS) -o $@ $^

obj/%.o: src/%.c
	$(COMPILER) $(CC_FLAGS) -c -o $@ $<

obj/%.o: src/test/%.c
	$(COMPILER) $(CC_FLAGS) -c -o $@ $<	

obj/%.o: ../core/src/%.c
	$(COMPILER) $(CC_FLAGS) -c -o $@ $<	
