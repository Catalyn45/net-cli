PROJNAME=netcli
TESTNAME=$(PROJNAME)_test

CC=clang
CXX=clang++

CFLAGS=-fPIC -g3 -Wall -Wextra -Werror -Winline -MD

LIBS=-lpthread
TEST_LIBS=-lgtest -lgtest_main

C=$(shell find ./src -name "*.c")
TEST_CPP=$(shell find ./test -name "*.cpp" 2> /dev/null)

OBJ=$(patsubst %,obj/%.o,$(basename $(C)))
TEST_OBJ=$(patsubst %,obj/%.o,$(basename $(TEST_CPP)))

default: all

obj/./src/%.o: src/%.c
	@mkdir -p $(@D)
	@echo -e "\033[0;32mCompiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

obj/./test/%.o: test/%.cpp
	@mkdir -p $(@D)
	@echo -e "\033[0;32mCompiling $<"
	@$(CXX) $(CFLAGS) -I . -c $< -o $@

$(PROJNAME): $(OBJ) 
	@echo -e "\033[0;36mLinking $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo -e "\033[0;35mSrc done"

$(TESTNAME): $(TEST_OBJ) $(filter-out obj/./src/main.o, $(OBJ))
	@echo -e "\033[0;36mLinking $@"
	@$(CXX) $(CFLAGS) -o $(TESTNAME) $^ $(LIBS) $(TEST_LIBS)
	@echo -e "\033[0;35mTest done"

all: $(PROJNAME) $(PROJNAME)_test
	@echo -e "\033[0;37mAll done"

clean:
	@echo -e "\033[1;33mCleaning up"
	@rm $(PROJNAME) -f
	@rm $(TESTNAME) -f
	@rm $(OBJ) -f
	@rm $(patsubst %.o, %.d, $(OBJ))
	@rm $(TEST_OBJ) -f
	@rm $(patsubst %.o, %.d, $(TEST_OBJ))

-include $(OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)
