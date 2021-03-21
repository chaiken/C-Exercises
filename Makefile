###############################################################################
##									     ##
##	File:     Makefile						     ##
##	Author:   Alison Chaiken					     ##
##	Created:  Mon May 18 15:16:12 EDT 1987                               ##
##	Contents: >>>Replace this with YOUR description of file contents<<<  ##
##									     ##
##	$Log$								     ##
###############################################################################

CBASICFLAGS = -O0 -fno-inline -g -ggdb -Wall -Wextra -fsanitize=address,undefined
CFLAGS = $(CBASICFLAGS) -isystem $(GTEST_DIR)/include
CDEBUGFLAGS = $(CFLAGS) -DDEBUG=1

GTEST_DIR = $(HOME)/gitsrc/googletest
GTEST_HEADERS = $(GTEST_DIR)/googletest/include
GTESTLIBPATH=$(GTEST_DIR)/build/lib
GTESTLIBS = $(GTESTLIBPATH)/libgtest.a $(GTESTLIBPATH)/libgtest_main.a
LDBASICFLAGS= -g -fsanitize=address,undefined -lpthread
LDFLAGS= $(LDBASICFLAGS) -L$(GTESTLIBPATH)
LDDEBUGFLAGS = $(LDFLAGS) -DDEBUG=1

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

palindrome: palindrome.c
	$(CCC) $(CFLAGS) $(LDFLAGS) -o palindrome palindrome.c

palindrome_test: palindrome_testsuite.o palindrome.c
	$(CPPCC) $(CFLAGS) $(LDFLAGS) -Wall -o "palindrome_test" palindrome_testsuite.o $(GTESTLIBS) -pthread

clean:
	/bin/rm -rf *.o *~ palindrome palindrome_test helloc
