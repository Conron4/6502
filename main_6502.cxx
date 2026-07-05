#include <stdio.h>
#include <stdlib.h>

using BYTE = unsigned char;
using WORD = unsigned short;

struct CPU{
    WORD PC; // Program Counter
    BYTE SP; // Stack Pointer
    BYTE A, X, Y;  // Accumulator, X Index Register, Y Index Register
    BYTE C : 1; // Carry Flag
    BYTE Z : 1; // Zero Flag
    BYTE I : 1; // Interrupt Disable
    BYTE D : 1; // Decimal Mode
    BYTE B : 1; // Break Command
    BYTE V : 1; // Overflow Flag
    BYTE N : 1; // Negative Flag
    void reset() {

    }
        
};

int main() {
    CPU cpu;
    cpu.reset();
    return 0;
}