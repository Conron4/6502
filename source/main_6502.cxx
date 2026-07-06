// 6502 Emulator
// Licensed under the GPL V3 License.
// Copyright (C) 2026 Connor Hopley
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>

using byte = unsigned char;
using word = unsigned short;
using u32 = unsigned int;

struct Mem {
    static const u32 MAX_MEM = 1024 * 48; // 48KB of memory
    byte data[MAX_MEM];
    void init() {
        for (u32 i = 0; i < MAX_MEM; ++i) {
            data[i] = 0;
        }
    };
    
    byte operator[](u32 addr) const {
        return data[addr];
    };

    byte & operator[](u32 addr) {
        return data[addr];
    }
};

struct Rom {
    static const u32 MAX_ROM = 1024 * 16; // 16KB of ROM
    byte data[MAX_ROM];
    void init() {
        for (u32 i = 0; i < MAX_ROM; ++i) {
            data[i] = 0;
        }
    };
    
    byte operator[](u32 addr) const {
        return data[addr];
    };

    byte & operator[](u32 addr) {
        return data[addr];
    }

    bool load_from_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "ROM Error: Could not open file " << filename << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        if (size != MAX_ROM) {
            std::cerr << "ROM Error: Size mismatch. Expected " << MAX_ROM 
                      << " bytes, got " << size << " bytes." << std::endl;
            return false;
        }

        file.seekg(0, std::ios::beg);
        if (file.read(reinterpret_cast<char*>(data), MAX_ROM)) {
            return true;
        } else {
            std::cerr << "ROM Error: Failed to read file data." << std::endl;
            return false;
        }
    }
};

// Unified Memory System that translates addresses automatically
struct Bus {
    Mem ram;
    Rom rom;

    // 16 pages of 4KB to cover the entire 64KB address space
    // We create separate read and write maps so ROM writes can point to a dead buffer
    const byte* read_map[16];
    byte* write_map[16];
    
    byte junk_page[4096]; // A throwaway buffer to absorb forbidden ROM writes safely

    void init() {
        ram.init();
        rom.init();
        for (int i = 0; i < 4096; ++i) junk_page[i] = 0;

        // Map the first 12 pages (0x0000 to 0xBFFF) directly to RAM
        // Each index represents a 4KB chunk
        for (int page = 0; page < 12; ++page) {
            read_map[page]  = &ram.data[page * 4096];
            write_map[page] = &ram.data[page * 4096];
        }

        // Map the remaining 4 pages (0xC000 to 0xFFFF) to ROM
        // We subtract 12 from the index so page 12 points to index 0 of ROM data
        for (int page = 12; page < 16; ++page) {
            read_map[page]  = &rom.data[(page - 12) * 4096];
            
            // CRITICAL SPEED TRICK: Point ROM writes to our junk page. 
            // The CPU can write to it all it wants; it won't affect ROM or cause an IF statement.
            write_map[page] = junk_page; 
        }
    }

    // Zero conditional branches! Just bit shifts and pointer math.
    inline byte read(word address) const {
        u32 page = address >> 12;         // Top 4 bits get the page index (0-15)
        u32 offset = address & 0x0FFF;    // Bottom 12 bits get the byte within that 4KB page
        return read_map[page][offset];
    }

    inline void write(word address, byte value) {
        u32 page = address >> 12;
        u32 offset = address & 0x0FFF;
        write_map[page][offset] = value;
    }
};

struct CPU {
    word PC; // Program Counter
    byte SP; // Stack Pointer
    
    byte A, X, Y;  // Accumulator, X Index Register, Y Index Register
    
    // STATUS REGISTER
    byte C : 1; // Carry Flag
    byte Z : 1; // Zero Flag
    byte I : 1; // Interrupt Disable
    byte D : 1; // Decimal Mode
    byte B : 1; // Break Command
    byte V : 1; // Overflow Flag
    byte N : 1; // Negative Flag

    // Opcodes
    static const byte
        INS_JMP_ABS = 0x4C,
        INS_LDA_IMM = 0xA9,
        INS_LDA_ZP  = 0xA5;
    
    // CPU now references the Bus instead of raw Mem
    void reset(Bus & bus) {
        PC = bus.read(0xFFFC) | (bus.read(0xFFFD) << 8); // Reads cleanly out of translated ROM!
        SP = 0x00; 
        A = X = Y = 0; 
        D = C = Z = I = B = V = N = 0; 
    }

    void LDASetStatusFlags() {
        Z = (A == 0);
        N = (A & 0x80) != 0;
    }

    void execute(u32 ticks, Bus & bus) {
        while (ticks > 0) {
            byte INS = fetch(ticks, bus);
            switch (INS) {
                case INS_LDA_IMM: {
                    byte value = fetch(ticks, bus);
                    A = value;
                    LDASetStatusFlags();
                    break;
                }
                case INS_LDA_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    A = ReadByte(ticks, zero_page_addr, bus);
                    LDASetStatusFlags();
                    break;
                }
                case INS_JMP_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    PC = addr;
                    break;
                }
                default:
                    printf("Unknown instruction: %02X\n", INS);
                    exit(1);
            }
        }
    }

    // Fetch the next byte
    byte fetch(u32 & ticks, Bus & bus) {
        byte instruction = bus.read(PC);
        PC++;
        ticks--;
        return instruction;
    }
    
    byte ReadByte(u32& ticks, word address, Bus & bus) {
        byte data = bus.read(address);
        ticks--;
        return data;
    }

    void WriteByte(u32& ticks, word address, byte data, Bus & bus) {
        bus.write(address, data);
        ticks--;
    }
};

int main() {
    Bus bus;
    CPU cpu;
    
    bus.init();
    
    // Setting up reset vector addresses in our simulated ROM space manually for test purposes
    // (In actual execution, you'd usually call bus.rom.load_from_file("apple2.rom"))
    bus.rom[0xFFFC - 0xC000] = 0x00; // Low Byte pointing to 0x0100
    bus.rom[0xFFFD - 0xC000] = 0x01; // High Byte
    
    // Populate an actual program in lower RAM space
    bus.ram[0x0100] = 0xA9; // INS_LDA_IMM
    bus.ram[0x0101] = 0x7F; // Value to load
    
    // Fire up the emulation pipeline
    cpu.reset(bus);
    cpu.execute(2, bus); // Executes the LDA operation
    
    return 0;
}