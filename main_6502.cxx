#include <stdio.h>
#include <stdlib.h>

using BYTE = unsigned char;
using WORD = unsigned short;
using u32 = unsigned int;

struct CPU{
    WORD PC; // Program Counter
    BYTE SP; // Stack Pointer
    
    BYTE A, X, Y;  // Accumulator, X Index Register, Y Index Register
    
    //STATUS REGISTER
    BYTE C : 1; // Carry Flag
    BYTE Z : 1; // Zero Flag
    BYTE I : 1; // Interrupt Disable
    BYTE D : 1; // Decimal Mode
    BYTE B : 1; // Break Command
    BYTE V : 1; // Overflow Flag
    BYTE N : 1; // Negative Flag
    
    void reset() {
        PC = 0xFFFC; // Reset vector address
        SP = 0x0100; // Stack Pointer initialized to 0x0100
        A = X = Y = 0; // Clear registers
        D = C = Z = I = B = V = N = 0; // Clear status flags
    }
        
};

struct mem {
    static const u32 MAX_MEM = 1024 * 64;
    BYTE data[MAX_MEM];
};

int main() {
    CPU cpu;
    cpu.reset();
    return 0;
}