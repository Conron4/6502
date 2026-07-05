#include <stdio.h>
#include <stdlib.h>

using BYTE = unsigned char;
using WORD = unsigned short;
using u32 = unsigned int;

struct Mem {
    static const u32 MAX_MEM = 1024 * 64;
    BYTE data[MAX_MEM];
    void init() {
        for (u32 i = 0; i < MAX_MEM; ++i) {
            data[i] = 0;
        }
    };
    BYTE operator[](u32 addr) const {
        return data[addr];
    };

    BYTE & operator[](u32 addr) {
        return data[addr];
    }
};

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

    static const BYTE
         INS_LDA_IMM = 0xA9; // Load Accumulator with Immediate
    
    void reset( Mem & memory) {
        PC = 0xFFFC; // Reset vector address
        SP = 0x0100; // Stack Pointer initialized to 0x0100
        A = X = Y = 0; // Clear registers
        D = C = Z = I = B = V = N = 0; // Clear status flags
        memory.init(); 
    }

    void execute(u32 ticks, Mem & memory) {
        while (ticks > 0) {
            BYTE INS = fetch( ticks, memory);
            switch (INS) {
                case INS_LDA_IMM: {
                    BYTE value = fetch( ticks, memory);
                    A = value;
                    Z = (A == 0);
                    N = (A & 0x80) != 0;
                    break;
                }
                default:
                    printf("Unknown instruction: %02X\n", INS);
                    exit(1);
            }
        }
    }

    BYTE fetch( u32 & ticks, Mem & memory) {
        BYTE instruction = memory.data[PC];
        PC++;
        ticks--;
        return instruction;
    }
        
};



int main() {
    Mem mem;
    CPU cpu;
    cpu.reset(mem);
    mem[0xFFFC] = 0xA9;
    mem[0xFFFD] = 0x42; // LDA #$42
    cpu.execute(2, mem);
    return 0;
}