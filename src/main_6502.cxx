// 6502 Emulator
// Licensed under the GPL V2 License.
// Copyright (C) 2026 Connor Hopley
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <SDL2/SDL.h>


using byte = unsigned char;
using word = unsigned short;
using u32 = unsigned int;


struct Mem {
    static const u32 MAX_MEM = 1024 * 44; // 44KB of memory
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
            std::cerr << "ROM Error: File size is " << size << " bytes, expected " << MAX_ROM << " bytes." << std::endl;
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

struct Vram {
    static const u32 MAX_VRAM = 1024 * 4; // 4KB of VRAM
    byte data[MAX_VRAM];
    void init() {
        for (u32 i = 0; i < MAX_VRAM; ++i) {
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

// Unified Memory System that translates addresses automatically
struct Bus {
    Mem ram;
    Rom rom;
    Vram vram;
    static const u32 PageSize = 4096; // 4KB pages for memory mapping
    // 16 pages of 4KB to cover the entire 64KB address space
    // We create separate read and write maps so ROM writes can point to a dead buffer
    const byte* read_map[16];
    byte* write_map[16];
    
    byte junk_page[PageSize]; // A throwaway buffer to absorb forbidden ROM writes safely

    void init() {
        ram.init();
        rom.init();
        vram.init();
        for (int i = 0; i < PageSize; ++i) junk_page[i] = 0;

        // Map the first 11 pages (0x0000 to 0xBFFF) directly to RAM
        // Each index represents a 4KB chunk
        for (int page = 0; page < 11; ++page) {
            read_map[page]  = &ram.data[page * PageSize];
            write_map[page] = &ram.data[page * PageSize];
        }
        for (int page = 11; page < 12; ++page) {
            read_map[page]  = &vram.data[(page - 11) * PageSize];
            write_map[page] = &vram.data[(page - 11) * PageSize];
        }
        // Map the remaining 4 pages (0xC000 to 0xFFFF) to ROM
        // We subtract 12 from the index so page 12 points to index 0 of ROM data
        for (int page = 12; page < 16; ++page) {
            read_map[page]  = &rom.data[(page - 12) * PageSize];
            
            // ROM writes pushed to junk page
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

    void render_screen() const {
    const int SCREEN_WIDTH = 640;
    const int SCREEN_HEIGHT = 400;
    const int SCALE = 2; // Scales window to 1280x800 so it's easy to see

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "6502 Emulator Display (80x50)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // This allows us to work in our raw 640x400 space while SDL handles scaling automatically
    SDL_RenderSetScale(renderer, SCALE, SCALE); 

    const word vram_start = 0xB000;
    const word char_rom_start = 0xC000;

    bool running = true;
    SDL_Event event;

    // Window event loop
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Clear display to a dark gray background
        SDL_SetRenderDrawColor(renderer, 0x1A, 0x1A, 0x1A, 0xFF);
        SDL_RenderClear(renderer);

        // Set draw color to a bright green for the pixels
        SDL_SetRenderDrawColor(renderer, 0x64, 0xFF, 0x64, 0xFF);

        // Unpack memory and draw pixels
        for (u32 row = 0; row < 50; ++row) {
            for (u32 pixel_row = 0; pixel_row < 8; ++pixel_row) {
                for (u32 col = 0; col < 80; ++col) {
                    
                    u32 vram_offset = (row * 80) + col;
                    byte char_index = read(vram_start + vram_offset);

                    word pixel_data_address = char_rom_start + (char_index * 8) + pixel_row;
                    byte row_bits = read(pixel_data_address);

                    for (int bit = 0; bit < 8; ++bit) {
                        bool is_pixel_on = (row_bits >> (7 - bit)) & 1;
                        
                        if (is_pixel_on) {
                            // Calculate absolute screen X and Y pixel coordinates
                            int screen_x = (col * 8) + bit;
                            int screen_y = (row * 8) + pixel_row;
                            SDL_RenderDrawPoint(renderer, screen_x, screen_y);
                        }
                    }
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // Cap at roughly 60 FPS to keep your CPU happy
    }

    // Clean up graphics objects when the window is closed
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
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
    byte processorstatus() const {
        return (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) | (Z << 1) | C;
    }

    // Opcodes
    static const byte
        // Load and Store Instructions
        // Load A
        INS_LDA_IMM = 0xA9,
        INS_LDA_ZP  = 0xA5,
        INS_LDA_ZPX = 0xB5,
        INS_LDA_ABS = 0xAD,
        INS_LDA_ABX = 0xBD,
        INS_LDA_ABY = 0xB9,
        INS_LDA_INX = 0xA1,
        INS_LDA_INY = 0xB1,
        // Load X
        INS_LDX_IMM = 0xA2,
        INS_LDX_ZP  = 0xA6,
        INS_LDX_ZPY = 0xB6,
        INS_LDX_ABS = 0xAE,
        INS_LDX_ABY = 0xBE,
        // Load Y
        INS_LDY_IMM = 0xA0,
        INS_LDY_ZP  = 0xA4,
        INS_LDY_ZPX = 0xB4,
        INS_LDY_ABS = 0xAC,
        INS_LDY_ABX = 0xBC,
        // Store A
        INS_STA_ZP  = 0x85,
        INS_STA_ZPX = 0x95,
        INS_STA_ABS = 0x8D,
        INS_STA_ABX = 0x9D,
        INS_STA_ABY = 0x99,
        // Store X
        INS_STX_ZP  = 0x86,
        INS_STX_ZPY = 0x96,
        INS_STX_ABS = 0x8E,
        // Store Y
        INS_STY_ZP  = 0x84,
        INS_STY_ZPX = 0x94,
        INS_STY_ABS = 0x8C,
        // Transfer Instructions
        INS_TAX      = 0xAA,
        INS_TAY      = 0xA8,
        INS_TXA      = 0x8A,
        INS_TYA      = 0x98,
        // Stack Instructions
        INS_TSX      = 0xBA,
        INS_TXS      = 0x9A,
        INS_PHA      = 0x48,
        INS_PHP      = 0x08,
        INS_PLA      = 0x68,
        INS_PLP      = 0x28,
        // LOGICAL
        // Logical AND
        INS_AND_IMM  = 0x29,
        INS_AND_ZP   = 0x25,
        INS_AND_ZPX  = 0x35,
        INS_AND_ABS  = 0x2D,
        INS_AND_ABX  = 0x3D,
        INS_AND_ABY  = 0x39,
        INS_AND_INX  = 0x21,
        INS_AND_INY  = 0x31,
        // Exclusive OR
        INS_EOR_IMM  = 0x49,
        INS_EOR_ZP   = 0x45,
        INS_EOR_ZPX  = 0x55,
        INS_EOR_ABS  = 0x4D,
        INS_EOR_ABX  = 0x5D,
        INS_EOR_ABY  = 0x59,
        INS_EOR_INX  = 0x41,
        INS_EOR_INY  = 0x51,
        // Inclusive OR
        INS_ORA_IMM  = 0x09,
        INS_ORA_ZP   = 0x05,
        INS_ORA_ZPX  = 0x15,
        INS_ORA_ABS  = 0x0D,
        INS_ORA_ABX  = 0x1D,
        INS_ORA_ABY  = 0x19,
        INS_ORA_INX  = 0x01,
        INS_ORA_INY  = 0x11,
        // Bit Test
        INS_BIT_ZP   = 0x24,
        INS_BIT_ABS  = 0x2C,
        // Arthmetic
        // ADD with Carry
        INS_ADC_IMM  = 0x69,
        INS_ADC_ZP   = 0x65,
        INS_ADC_ZPX  = 0x75,
        INS_ADC_ABS  = 0x6D,
        INS_ADC_ABX  = 0x7D,
        INS_ADC_ABY  = 0x79,
        INS_ADC_INX  = 0x61,
        INS_ADC_INY  = 0x71,
        // SUB with Carry
        INS_SBC_IMM  = 0xE9,
        INS_SBC_ZP   = 0xE5,
        INS_SBC_ZPX  = 0xF5,
        INS_SBC_ABS  = 0xED,
        INS_SBC_ABX  = 0xFD,
        INS_SBC_ABY  = 0xF9,
        INS_SBC_INX  = 0xE1,
        INS_SBC_INY  = 0xF1,
        // CMP Accumulator
        INS_CMP_IMM  = 0xC9,
        INS_CMP_ZP   = 0xC5,
        INS_CMP_ZPX  = 0xD5,
        INS_CMP_ABS  = 0xCD,
        INS_CMP_ABX  = 0xDD,
        INS_CMP_ABY  = 0xD9,
        INS_CMP_INX  = 0xC1,
        INS_CMP_INY  = 0xD1,
        // CMP X Register
        INS_CPX_IMM  = 0xE0,
        INS_CPX_ZP   = 0xE4,
        INS_CPX_ABS  = 0xEC,
        // CMP Y Register
        INS_CPY_IMM  = 0xC0,
        INS_CPY_ZP   = 0xC4,
        INS_CPY_ABS  = 0xCC,
        // INC / DEC
        // Increment
        INS_INC_ZP   = 0xE6,
        INS_INC_ZPX  = 0xF6,
        INS_INC_ABS  = 0xEE,
        INS_INC_ABX  = 0xFE,
        // Increment X/Y
        INS_INX      = 0xE8,
        INS_INY      = 0xC8,
        // Decrement
        INS_DEC_ZP   = 0xC6,
        INS_DEC_ZPX  = 0xD6,
        INS_DEC_ABS  = 0xCE,
        INS_DEC_ABX  = 0xDE,
        // Decrement X/Y
        INS_DEX      = 0xCA,
        INS_DEY      = 0x88,
        // Shifts
        // Arithmetic Shift Left
        INS_ASL_ACC  = 0x0A,
        INS_ASL_ZP   = 0x06,
        INS_ASL_ZPX  = 0x16,
        INS_ASL_ABS  = 0x0E,
        INS_ASL_ABX  = 0x1E,
        // Logical Shift Right
        INS_LSR_ACC  = 0x4A,
        INS_LSR_ZP   = 0x46,
        INS_LSR_ZPX  = 0x56,
        INS_LSR_ABS  = 0x4E,
        INS_LSR_ABX  = 0x5E,
        // Rotate Left
        INS_ROL_ACC  = 0x2A,
        INS_ROL_ZP   = 0x26,
        INS_ROL_ZPX  = 0x36,
        INS_ROL_ABS  = 0x2E,
        INS_ROL_ABX  = 0x3E,
        // Rotate Right
        INS_ROR_ACC  = 0x6A,
        INS_ROR_ZP   = 0x66,
        INS_ROR_ZPX  = 0x76,
        INS_ROR_ABS  = 0x6E,
        INS_ROR_ABX  = 0x7E,
        // Jumps & Calls
        // Jump to location
        INS_JMP_ABS = 0x4C,
        INS_JMP_IND = 0x6C,
        // Jump to subroutine
        INS_JSR_ABS = 0x20,
        // Return from subroutine
        INS_RTS_IMP = 0x60,
        //Branches
        INS_BCC     = 0x90,
        INS_BCS     = 0xB0,
        INS_BEQ     = 0xF0,
        INS_BMI     = 0x30,
        INS_BNE     = 0xD0,
        INS_BPL     = 0x10,
        INS_BVC     = 0x50,
        INS_BVS     = 0x70,
        // Status flag changes
        INS_CLC     = 0x18,
        INS_CLD     = 0xD8,
        INS_CLI     = 0x58,
        INS_CLV     = 0xB8,
        INS_SEC     = 0x38,
        INS_SED     = 0xF8,
        INS_SEI     = 0x78,
        // System Functions
        INS_BRK     = 0x00,
        INS_NOP     = 0xEA,
        INS_RTI     = 0x40;
    // CPU now references the Bus instead of raw Mem
    void reset(Bus & bus) {
        PC = bus.read(0xFFFC) | ((word)bus.read(0xFFFD) << 8); // Reads cleanly out of translated ROM!
        SP = 0xFF; 
        A = X = Y = 0; 
        D = C = Z = I = B = V = N = 0; 
    }

    void LDSetStatusFlags(byte value) {
        Z = (value == 0);
        N = (value & 0x80) != 0;
    }

    void execute(u32 ticks, Bus & bus) {
        while (ticks > 0) {
            byte INS = fetch(ticks, bus);
            switch (INS) {
                case INS_LDA_IMM: {
                    byte value = fetch(ticks, bus);
                    A = value;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    A = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    A = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    A = ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABX: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    A = ReadByte(ticks, addr + X, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABY: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    A = ReadByte(ticks, addr + Y, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    A = ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    A = ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDX_IMM: {
                    byte value = fetch(ticks, bus);
                    X = value;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    X = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ZPY: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + Y;
                    X = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    X = ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ABY: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    X = ReadByte(ticks, addr + Y, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDY_IMM: {
                    byte value = fetch(ticks, bus);
                    Y = value;
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    Y = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ZPX: {     
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    Y = ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    Y = ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ABX: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    Y = ReadByte(ticks, addr + X, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_STA_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    WriteByte(ticks, zero_page_addr, A, bus);
                    break;
                }
                case INS_STA_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    WriteByte(ticks, zero_page_addr, A, bus);
                    break;
                }
                case INS_STA_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    WriteByte(ticks, addr, A, bus);
                    break;
                }
                case INS_STA_ABX: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    WriteByte(ticks, addr + X, A, bus);
                    break;
                }
                case INS_STA_ABY: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    WriteByte(ticks, addr + Y, A, bus);
                    break;
                }
                case INS_STX_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    WriteByte(ticks, zero_page_addr, X, bus);
                    break;
                }
                case INS_STX_ZPY: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + Y;
                    WriteByte(ticks, zero_page_addr, X, bus);
                    break;
                }
                case INS_STX_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    WriteByte(ticks, addr, X, bus);
                    break;
                }
                case INS_STY_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    WriteByte(ticks, zero_page_addr, Y, bus);
                    break;
                }
                case INS_STY_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    WriteByte(ticks, zero_page_addr, Y, bus);
                    break;
                }
                case INS_STY_ABS: {
                    word addr = fetch(ticks, bus); // Low byte
                    addr |= ((word)fetch(ticks, bus)) << 8; // High byte
                    WriteByte(ticks, addr, Y, bus);
                    break;
                }
                case INS_TAX: {
                    X = A;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_TAY: {
                    Y = A;
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_TXA: {
                    A = X;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_TYA: {
                    A = Y;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_TSX: {
                    X = SP;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_TXS: {
                    SP = X;
                    break;
                }
                case INS_PHA: {
                    WriteByte(ticks, SP + 0x100, A, bus);
                    SP--;
                    break;
                }
                case INS_PHP: {
                    byte PS = processorstatus() | 0x10;
                    WriteByte(ticks, SP + 0x100, PS, bus);
                    SP--;
                    break;
                }
                case INS_PLA: {
                    SP++;
                    A = ReadByte(ticks, SP + 0x100, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_PLP: {
                    SP++;
                    byte PS = ReadByte(ticks, SP + 0x100, bus);
                    N = (PS >> 7) & 1;
                    V = (PS >> 6) & 1;
                    B = (PS >> 4) & 1;
                    D = (PS >> 3) & 1;
                    I = (PS >> 2) & 1;
                    Z = (PS >> 1) & 1;
                    C = (PS >> 0) & 1;
                    break;
                }
                case INS_AND_IMM: {
                    byte opr = fetch(ticks, bus);
                    A = A&opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    A = A&ReadByte(ticks, zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    A = A&ReadByte(ticks,zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABS: {
                    word addr = wordfetch(ticks, bus);
                    A = A&ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABX: {
                    word addr = wordfetch(ticks, bus);
                    addr = addr + X;
                    A = A&ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABY: {
                    word addr = wordfetch(ticks, bus);
                    addr = addr + Y;
                    A = A&ReadByte(ticks, addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    A = A&ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    A = A&ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_IMM: {
                    byte opr = fetch(ticks, bus);
                    A = A^opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ZP: {
                    byte zero_page_addr = fetch(ticks, bus);
                    A = A^ReadByte(ticks,zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    A = A^ReadByte(ticks,zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABS: {
                    word addr = wordfetch(ticks,bus);
                    A = A^ReadByte(ticks,addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    A = A^ReadByte(ticks,addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABY: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + Y;
                    A = A^ReadByte(ticks, addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    A = A^ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    A = A^ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_IMM:{
                    byte opr = fetch(ticks, bus);
                    A = A|opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    A = A|ReadByte(ticks,zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr+X;
                    A = A|ReadByte(ticks,zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABS: {
                    word addr = wordfetch(ticks,bus);
                    A = A|ReadByte(ticks,addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    A = A|ReadByte(ticks,addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABY: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + Y;
                    A = A|ReadByte(ticks,addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    A = A|ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    A = A|ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_BIT_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte value = ReadByte(ticks,zero_page_addr,bus);
                    Z = ((A & value) == 0);
                    N = (value >> 7) & 1;
                    V = (value >> 6) & 1;
                    break;

                }
                case INS_BIT_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte value = ReadByte(ticks,addr,bus);
                    Z = ((A & value) == 0);
                    N = (value >> 7) & 1;
                    V = (value >> 6) & 1;
                    break;

                }
                case INS_ADC_IMM: {
                    byte opr = fetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, opr, false,bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, opr, false, bus);
                    }
                    break;
                }
                case INS_ADC_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,zero_page_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,zero_page_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,zero_page_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,zero_page_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ABS: {
                    word addr = wordfetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), false, bus);
                    }
                    break;

                }
                case INS_ADC_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ABY: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + Y;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,effective_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,effective_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,effective_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,effective_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_SBC_IMM: {
                    byte opr = fetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, opr, true,bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, opr, true, bus);
                    }
                    break;
                }
                case INS_SBC_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,zero_page_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,zero_page_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,zero_page_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,zero_page_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ABS: {
                    word addr = wordfetch(ticks,bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), true, bus);
                    }
                    break;

                }
                case INS_SBC_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ABY: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + Y;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_INX: {
                    word effective_addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,effective_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,effective_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_INY: {
                    word effective_addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ticks, ReadByte(ticks,effective_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ticks, ReadByte(ticks,effective_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_CMP_IMM: {
                    byte opr = fetch(ticks, bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte opr = ReadByte(ticks,zero_page_addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte opr = ReadByte(ticks,zero_page_addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABY: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + Y;
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_INX: {
                    word addr = indexed_indirect(ticks, fetch(ticks, bus), bus);
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_INY: {
                    word addr = indirect_indexed(ticks, fetch(ticks, bus), bus);
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_IMM: {
                    byte opr = fetch(ticks, bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte opr = ReadByte(ticks,zero_page_addr,bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_IMM: {
                    byte opr = fetch(ticks, bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte opr = ReadByte(ticks,zero_page_addr,bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte opr = ReadByte(ticks,addr,bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_INC_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte inc_tmp = ReadByte(ticks, zero_page_addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(ticks, zero_page_addr,inc_tmp,bus);
                    break;
                }
                case INS_INC_ZPX: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zero_page_addr + X;
                    byte inc_tmp = ReadByte(ticks, zero_page_addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(ticks, zero_page_addr, inc_tmp, bus);
                    break;
                }
                case INS_INC_ABS: {
                    word addr = wordfetch(ticks, bus);
                    byte inc_tmp = ReadByte(ticks, addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(ticks, addr, inc_tmp, bus);
                    break;
                }
                case INS_INC_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    byte inc_tmp = ReadByte(ticks,addr,bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(ticks, addr, inc_tmp, bus);
                    break;
                }
                case INS_INX: {
                    X++;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_INY: {
                    Y++;
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_DEC_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte dec_tmp = ReadByte(ticks, zero_page_addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(ticks, zero_page_addr,dec_tmp,bus);
                    break;
                }
                case INS_DEC_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte dec_tmp = ReadByte(ticks, zero_page_addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(ticks, zero_page_addr,dec_tmp,bus);
                    break;
                }
                case INS_DEC_ABS: {
                    word addr = wordfetch(ticks, bus);
                    byte dec_tmp = ReadByte(ticks, addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(ticks, addr, dec_tmp, bus);
                    break;
                }
                case INS_DEC_ABX: {
                    word addr = wordfetch(ticks, bus);
                    addr = addr + X;
                    byte dec_tmp = ReadByte(ticks, addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(ticks, addr, dec_tmp, bus);
                    break;
                }
                case INS_DEX: {
                    X--;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_DEY: {
                    Y--;
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_ASL_ACC: {
                    C = (A >> 7) & 1;
                    A = A << 1;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ASL_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(ticks,zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_ASL_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(ticks,zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_ASL_ABS: {
                    word addr = wordfetch(ticks, bus);
                    byte tmp = ReadByte(ticks,addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(ticks,addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ASL_ABX: {
                    word addr = wordfetch(ticks, bus);
                    addr = addr + X;
                    byte tmp = ReadByte(ticks,addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(ticks,addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_LSR_ACC: {
                    C = A & 1;
                    A = A >> 1;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LSR_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(ticks,zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_LSR_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(ticks,zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_LSR_ABS: {
                    word addr = wordfetch(ticks, bus);
                    byte tmp = ReadByte(ticks,addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(ticks,addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_LSR_ABX: {
                    word addr = wordfetch(ticks, bus);
                    addr = addr + X;
                    byte tmp = ReadByte(ticks,addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(ticks,addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ACC: {
                    byte tmpC = (A >> 7) & 1;
                    A = A << 1;
                    A |= C;
                    C = tmpC;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ROL_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(ticks, zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(ticks,zero_page_addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(ticks, zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte tmp = ReadByte(ticks,addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(ticks, addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    byte tmp = ReadByte(ticks,addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(ticks, addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ACC: {
                    byte tmpC = A  & 1;
                    A = A >> 1;
                    A |= (C << 7);
                    C = tmpC;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ROR_ZP: {
                    byte zero_page_addr = fetch(ticks,bus);
                    byte tmp = ReadByte(ticks, zero_page_addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(ticks, zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ZPX: {
                    byte zero_page_addr = fetch(ticks,bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(ticks, zero_page_addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(ticks, zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ABS: {
                    word addr = wordfetch(ticks,bus);
                    byte tmp = ReadByte(ticks, addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(ticks, addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ABX: {
                    word addr = wordfetch(ticks,bus);
                    addr = addr + X;
                    byte tmp = ReadByte(ticks, addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(ticks, addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_JMP_ABS: {
                    word addr = wordfetch(ticks, bus);
                    PC = addr;
                    break;
                }
                case INS_JMP_IND: {
                    word addr = wordfetch(ticks, bus);
                    PC = ReadWordWithPageWrapBug(ticks, addr, bus);
                    break;
                }
                case INS_JSR_ABS: {
                    word target_addr = wordfetch(ticks, bus);
                    word return_addr = PC - 1;
                    WriteByte(ticks, 0x0100 + SP, (return_addr >> 8) & 0xFF, bus);
                    SP--;
                    WriteByte(ticks, 0x0100 + SP, return_addr & 0xFF, bus);
                    SP--;
                    PC = target_addr;
                    break;
                }
                case INS_RTS_IMP: {
                    SP++;
                    word return_addr_low = ReadByte(ticks, 0x0100 + SP, bus);
                    SP++;
                    word return_addr_high = ReadByte(ticks, 0x0100 + SP, bus);
                    
                    word return_addr = return_addr_low | (return_addr_high << 8);
                    PC = return_addr + 1;
                    break;
                }
                case INS_BCC: {
                    branch_relative(ticks, bus, C == 0);
                    break;
                }
                case INS_BCS: {
                    branch_relative(ticks, bus, C != 0);
                    break;
                }
                case INS_BEQ: {
                    branch_relative(ticks, bus, Z != 0);
                    break;
                }
                case INS_BMI: {
                    branch_relative(ticks, bus, N != 0);
                    break;
                }
                case INS_BNE: {
                    branch_relative(ticks, bus, Z == 0);
                    break;
                }
                case INS_BPL: {
                    branch_relative(ticks, bus, N == 0);
                    break;
                }
                case INS_BVC: {
                    branch_relative(ticks, bus, V == 0);
                    break;
                }
                case INS_BVS: {
                    branch_relative(ticks, bus, V != 0);
                    break;
                }
                case INS_CLC: {
                    C = 0;
                    break;
                }
                case INS_CLD: {
                    D = 0;
                    break;
                }
                case INS_CLI: {
                    I = 0;
                    break;
                }
                case INS_CLV: {
                    V = 0;
                    break;
                }
                case INS_SEC: {
                    C = 1;
                    break;
                }
                case INS_SED: {
                    D = 1;
                    break;
                }
                case INS_SEI: {
                    I = 1;
                    break;
                }
                case INS_BRK: {
                    PC++;
                    
                    WriteByte(ticks, 0x0100 + SP, (PC >> 8) & 0xFF, bus);
                    SP--;

                    
                    WriteByte(ticks, 0x0100 + SP, PC & 0xFF, bus);
                    SP--;
                    
                    byte stack_P = processorstatus() | 0x10 | 0x20; 
                    WriteByte(ticks, 0x0100 + SP, stack_P, bus);
                    SP--;
                    I = 1;
                    byte target_low = ReadByte(ticks, 0xFFFE, bus);
                    byte target_high = ReadByte(ticks, 0xFFFF, bus);
    
                    PC = target_low | ((word)target_high << 8);
                    break;
                }
                case INS_NOP: {
                    break;
                }
                case INS_RTI: {
                    //Step up to the Status Register slot and pull it
                    SP++;
                    byte pulled_P = ReadByte(ticks, 0x0100 + SP, bus);
    
                    // Unpack the pulled byte directly back into individual CPU flags.
                    C = (pulled_P >> 0) & 1;
                    Z = (pulled_P >> 1) & 1;
                    I = (pulled_P >> 2) & 1;
                    D = (pulled_P >> 3) & 1;
                    V = (pulled_P >> 6) & 1;
                    N = (pulled_P >> 7) & 1;

                    // Pull low PC
                    SP++;
                    word pc_low = ReadByte(ticks, 0x0100 + SP, bus);

                    // Pull high PC
                    SP++;
                    word pc_high = ReadByte(ticks, 0x0100 + SP, bus);

                    // Combine them into PC
                    PC = pc_low | (pc_high << 8);

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
    word wordfetch(u32 & ticks, Bus & bus) {
        word addr = fetch(ticks, bus); // Low byte
        addr |= ((word)fetch(ticks, bus)) << 8; // High byte
        return addr;
    }

    // NMOS 6502 quirk: JMP ($xxFF) reads high byte from $xx00, not $(xx+1)00.
    word ReadWordWithPageWrapBug(u32 & ticks, word pointer, Bus & bus) {
        byte low = ReadByte(ticks, pointer, bus);
        word high_addr = (pointer & 0xFF00) | ((pointer + 1) & 0x00FF);
        byte high = ReadByte(ticks, high_addr, bus);
        return low | ((word)high << 8);
    }

    void branch_relative(u32 & ticks, Bus & bus, bool should_branch) {
        signed char offset = static_cast<signed char>(fetch(ticks, bus));
        if (should_branch) {
            PC = static_cast<word>(PC + offset);
        }
    }

    // ($nn, X)
    word indexed_indirect(u32 & ticks, word addr, Bus & bus) {
        byte base_zp_addr = (byte)(addr & 0xFF); 

        byte low_zp = (base_zp_addr + X) & 0xFF;
        byte effective_addr_low = ReadByte(ticks, low_zp, bus);

        byte high_zp = (base_zp_addr + X + 1) & 0xFF;
        byte effective_addr_high = ReadByte(ticks, high_zp, bus);

        return effective_addr_low | ((word)effective_addr_high << 8);
    }

    // ($nn),Y
    word indirect_indexed(u32 & ticks, word addr, Bus & bus) {
        byte base_zp_addr = (byte)(addr & 0xFF); // Ensure it's treated as a ZP byte

        byte effective_addr_low = ReadByte(ticks, base_zp_addr, bus);
        // Wrap high byte read to zero page if base_zp_addr is 0xFF
        byte effective_addr_high = ReadByte(ticks, (base_zp_addr + 1) & 0xFF, bus); 
    
        word base_address = effective_addr_low | ((word)effective_addr_high << 8);
    
        // Add Y to the 16-bit base address here!
        return base_address + Y; 
    }
    byte ADC_SBC_HEX(u32 & ticks, byte opr, bool is_subtraction, Bus & bus) {
        if (is_subtraction) {
            opr = ~opr;
        }
        word result = A + opr + C;
        // If result > 0xFF set to 1 else set to 0
        C = (result > 0xFF) ? 1 : 0;
        // Check if A & opr have same sign bit and A & result have diffrent sign bits(7) 
        // Set V to 1 else set to 0
        V = (~(A ^ opr) & (A ^ result) & 0x80) ? 1 : 0;
        return (byte)result;
    }
    byte ADC_SBC_BCD(u32 & ticks, byte opr, bool is_subtraction, Bus & bus) {
        if (is_subtraction) {
            opr = ~opr;
        }
        // 1. Calculate the lower nibble (ones place)
        word low_nibble = (A & 0x0F) + (opr & 0x0F) + C;
        if (low_nibble > 9) {
            low_nibble += 6; // BCD Correction for low nibble
        }

        // 2. Calculate the upper nibble (tens place) using the low nibble's carry status
        // If low_nibble > 15, it naturally carried into bit 4. 
        word high_nibble = (A & 0xF0) + (opr & 0xF0) + (low_nibble & 0xF0);

        // 3. The Overflow flag (V) on the NMOS 6502 is calculated based on the 
        // SIGN of the inputs before BCD correction takes place.
        word binary_result = A + opr + C;
        V = (~(A ^ opr) & (A ^ binary_result) & 0x80) ? 1 : 0;

        // 4. Update the Zero (Z) and Negative (N) flags based on the standard binary result
        LDSetStatusFlags((byte)(binary_result & 0xFF));

        // 5. Check for high nibble decimal overflow
        if (high_nibble > 0x90) {
            high_nibble += 0x60; // BCD Correction for high nibble
            C = 1;               // Set carry out
        } else {
            C = 0;               // Clear carry out
        }

        // 6. Combine the corrected low and high nibbles
        byte result = (high_nibble & 0xF0) | (low_nibble & 0x0F);
    
        return result;
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

void setup_vram_test_pattern(Bus &bus) {
    const word vram_start = 0xB000;
    for (u32 i = 0; i < 80; ++i) {
        bus.write(vram_start + i, 0x20); // Fill the first row with spaces
    }
    // 1. Fill the entire 4,000 byte screen with a repeating cycle of glyphs
    for (u32 i = 80; i < 4000; ++i) {
        // This cycles character indices 0 through 63 repeatedly across the grid
        bus.write(vram_start + i, static_cast<byte>(i % 64));
    }

    // 2. Write a test message to the top-left corner of the screen
    std::string message = "6502 EMULATOR OK";
    for (size_t i = 0; i < message.length(); ++i) {
        char c = message[i];
        byte char_index = 0;

        if (c >= 'A' && c <= 'Z') {
            char_index = (c - 'A') + 1; // Map 'A' to 1, 'B' to 2, etc.
        } else if (c >= '0' && c <= '9') {
            char_index = (c - '0') + 48; // Common offset for numbers in C64/C16 ROMs
        } else {
            char_index = 0x20; // Space / alternative blank index
        }

        bus.write(vram_start + i, char_index);
    }
}

int main() {
    Bus bus;
    CPU cpu;
    
    bus.init();
    
    if (!bus.rom.load_from_file("rom.bin")) {
        std::cerr << "Failed to load ROM file." << std::endl;
        return 1;
    }
    byte low_byte = bus.read(0xFFFC);
    byte high_byte = bus.read(0xFFFD);
    
    std::cout << "--- ROM Diagnostics ---" << std::endl;
    std::cout << "File byte at 0xFFFC (index 0x3FFC): 0x" << std::hex << (int)low_byte << std::endl;
    std::cout << "File byte at 0xFFFD (index 0x3FFD): 0x" << std::hex << (int)high_byte << std::endl;
    // Fill VRAM with test indices
    setup_vram_test_pattern(bus);
    
    // Run the tile graphics renderer
    std::cout << "Rendering 80x50 pixel canvas..." << std::endl;
    bus.render_screen();
    
    cpu.reset(bus);
    cpu.execute(4, bus); // Executes the LDA operation
    
    return 0;
}