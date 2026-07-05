// 6502 Emulator
// Licensed under the GPL V3 License.
#include <stdio.h>
#include <stdlib.h>

using BYTE = unsigned char;
using WORD = unsigned short;
using u32 = unsigned int;

struct Mem {
    static const u32 MAX_MEM = 1024 * 64;
    BYTE data[MAX_MEM];
   // void init() {
   //     for (u32 i = 0; i < MAX_MEM; ++i) {
   //         data[i] = 0;
   //     }
   // };
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

    //Opcodes
    static const BYTE
        INS_JMP_ABS = 0x4C, // Jump to Address
        INS_LDA_IMM = 0xA9, // Load Accumulator with Immediate
        INS_LDA_ZP  = 0xA5; // Load Accumulator from Zero Page
    
    void reset( Mem & memory) {
        PC = memory[0xFFFC] | (memory[0xFFFD] << 8); // Reset vector address
        SP = 0x00; // Stack Pointer initialized to 0x00
        A = X = Y = 0; // Clear registers
        D = C = Z = I = B = V = N = 0; // Clear status flags
    }
    void LDASetStatusFlags() {
        Z = (A == 0);
        N = (A & 0x80) != 0;
    }

    void execute(u32 ticks, Mem & memory) {
        while (ticks > 0) {
            BYTE INS = fetch( ticks, memory);
            switch (INS) {
                case INS_LDA_IMM: {
                    BYTE value = fetch( ticks, memory);
                    A = value;
                    LDASetStatusFlags();
                    break;
                }
                case INS_LDA_ZP: {
                    BYTE zero_page_addr = fetch( ticks, memory);
                    A = ReadByte(ticks, zero_page_addr, memory);
                    LDASetStatusFlags();
                    break;
                }
                case INS_JMP_ABS: {
                    WORD addr = fetch(ticks, memory); // Low byte
                    addr |= ((WORD)fetch(ticks, memory)) << 8; // High byte
                    PC = addr;
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
    BYTE ReadByte(u32& ticks, BYTE address, Mem & memory) {
        BYTE Data = memory[address];
        ticks--;
        return Data;
    }
    BYTE WriteByte(u32& ticks, BYTE address, BYTE data, Mem & memory) {
        memory[address] = data;
        ticks--;
        return data;
    }
        
};



int main() {
    Mem mem;
    CPU cpu;
    //Reset Vector
    mem[0xFFFC] = 0x00; // Low byte
    mem[0xFFFD] = 0x01; // High byte
    mem[0x0100] = 0xA9; // LDA 
    mem[0x0101] = 0x7F; 
    cpu.reset(mem);
    mem[0x00FF] = 0x01;
    cpu.execute(2, mem);
    return 0;
}