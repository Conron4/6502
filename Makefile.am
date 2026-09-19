bin_PROGRAMS = bin/main_6502
check_PROGRAMS = bin/opcode_tests
TESTS = bin/opcode_tests
bin_main_6502_SOURCES = src/main_6502.cxx
bin_main_6502_CXXFLAGS = $(SDL2_CFLAGS)
bin_main_6502_LDADD = $(SDL2_LIBS)
bin_opcode_tests_SOURCES = src/opcode_tests.cxx
bin_opcode_tests_CXXFLAGS = $(SDL2_CFLAGS)
bin_opcode_tests_LDADD = $(SDL2_LIBS)

.PHONY: test
test: bin/main_6502
	$(MAKE) bin/opcode_tests
	./bin/opcode_tests