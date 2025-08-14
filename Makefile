###############################################################################
##									     ##
##	File:     Makefile						     ##
##	Author:   Alison Chaiken					     ##
##	Created:  Mon May 18 15:16:12 EDT 1987                               ##
##	Contents: >>>Replace this with YOUR description of file contents<<<  ##
##									     ##
##	$Log$								     ##
###############################################################################

CBASICFLAGS = -O0 -fno-inline -g -ggdb -Wall -Wextra -Werror -isystem $(GTEST_DIR)/include
CFLAGS = $(CBASICFLAGS) -fsanitize=address,undefined
CDEBUGFLAGS = $(CFLAGS) -DDEBUG=1

GTEST_DIR = $(HOME)/gitsrc/googletest
GTEST_HEADERS = $(GTEST_DIR)/googletest/include
GTESTLIBPATH=$(GTEST_DIR)/build/lib
GTESTLIBS = $(GTESTLIBPATH)/libgtest.a $(GTESTLIBPATH)/libgtest_main.a
LDBASICFLAGS= -g -L$(GTESTLIBPATH)
LDFLAGS= $(LDBASICFLAGS) -fsanitize=address,undefined
LDDEBUGFLAGS = $(LDFLAGS) -DDEBUG=1
#https://gist.github.com/kwk/4171e37f4bcdf7705329
#ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer

CCC = /usr/bin/gcc
CPPCC = /usr/bin/g++

# Each subdirectory must supply rules for building sources it contributes
%.o: %.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(CPPCC) -isystem $(GTEST_HEADERS) $(CFLAGS) -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# helloc_testsuite.cc says "#include "myapp.c".  Therefore myapp.o is not part
# of the explicit dependencies of helloc.   myapp.o will not link with -DTESTING
helloc: helloc_testsuite.o
	@echo 'Building target: $@'
	@echo 'Invoking: MacOS X C++ Linker'
	$(CPPCC) $(LDFLAGS) -Wall -o "helloc" helloc_testsuite.o $(GTESTLIBS) -pthread
#	g++ -I -L/usr/local/lib -Wall -o "helloc" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

palindrome_test: palindrome_testsuite.o palindrome.c
	$(CPPCC) $(CFLAGS) $(LDFLAGS)  -o "palindrome_test" palindrome_testsuite.o $(GTESTLIBS) -pthread

kernel-doubly-linked-macros: kernel-doubly-linked-macros.c
	$(CCC) $(CFLAGS) $(LDFLAGS) -o kernel-doubly-linked-macros kernel-doubly-linked-macros.c

kernel-doubly-linked-macros-valgrind: kernel-doubly-linked-macros.c
	$(CCC) $(CBASICFLAGS) $(LDBASICFLAGS) -o kernel-doubly-linked-macros-valgrind kernel-doubly-linked-macros.c
	valgrind kernel-doubly-linked-macros-valgrind

reverse-list_test: reverse-list_testsuite.o reverse-list.c
	$(CPPCC) $(CFLAGS) $(LDFLAGS)  -o reverse-list_test reverse-list_testsuite.o $(GTESTLIBS)

reverse-list-valgrind: reverse-list.c
	$(CCC) $(CBASICFLAGS) $(LDBASICFLAGS) -o reverse-list-valgrind reverse-list.c
	valgrind reverse-list-valgrind

matrix-determinant: matrix-determinant.c
	$(CCC) $(CFLAGS) $(LDFLAGS) -o matrix-determinant matrix-determinant.c -lm

matrix-determinant-valgrind: matrix-determinant.c
	$(CCC) $(CBASICFLAGS) $(LDBASICFLAGS) -o matrix-determinant-valgrind matrix-determinant.c -lm
	valgrind matrix-determinant-valgrind

matrix-determinant_test: matrix-determinant_testsuite.o matrix-determinant.c
	$(CPPCC) $(CFLAGS) $(LDFLAGS)  -o matrix-determinant_test matrix-determinant_testsuite.o $(GTESTLIBS)

cdecl: cdecl.c
	$(CCC) $(CFLAGS) $(LDFLAGS) -o cdecl cdecl.c

cdecl-valgrind: cdecl.c
	$(CCC) $(CBASICFLAGS) $(LDBASICFLAGS) -o cdecl-valgrind cdecl.c
	valgrind cdecl-valgrind

cdecl_test: cdecl_testsuite.o cdecl.c
	$(CPPCC) $(CFLAGS) $(LDFLAGS)  -o cdecl_test cdecl_testsuite.o $(GTESTLIBS)

clean:
	/bin/rm -rf *.o *~ *.d *test *-valgrind palindrome palindrome_test helloc matrix-determinant cdecl kernel-doubly-linked-macros

