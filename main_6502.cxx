#include <stdio.h>
#include <stdlib.h>

using BYTE = unsigned char;
using WORD = unsigned short;

struct CPU{
    WORD PC; // Program Counter
    BYTE SP; // Stack Pointer
    BYTE A, X, Y;  // Accumulator, X Index Register, Y Index Register
    BYTE P; // Processor Status Register
    void reset() {
        
    }
        
};

int main() {
    CPU cpu;
    cpu.reset();
    return 0;
}