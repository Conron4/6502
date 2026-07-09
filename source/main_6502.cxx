// 6502 Emulator
// Licensed under the GPL V3 License.
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
        INS_JMP_ABS = 0x4C,
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
        INS_PLP      = 0x28;
    
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
    word zeropage_bug(word addr) {
        if (addr > 0xFF){
            addr = addr & 0xFF; // Wrap around to zero page
        }
        return addr;
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
                    zero_page_addr = zeropage_bug(zero_page_addr + X);
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
                    byte zero_page_addr = fetch(ticks, bus);
                    byte effective_addr_low = ReadByte(ticks, (zero_page_addr + X) & 0xFF, bus);
                    byte effective_addr_high = ReadByte(ticks, (zero_page_addr + X + 1) & 0xFF, bus);
                    word effective_addr = effective_addr_low | ((word)effective_addr_high << 8);
                    A = ReadByte(ticks, effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_INY: {
                    byte zero_page_addr = fetch(ticks, bus);
                    byte effective_addr_low = ReadByte(ticks, zero_page_addr, bus);
                    byte effective_addr_high = ReadByte(ticks, (zero_page_addr + 1) & 0xFF, bus);
                    word effective_addr = effective_addr_low | ((word)effective_addr_high << 8);
                    A = ReadByte(ticks, effective_addr + Y, bus);
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
                    break;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ZPY: {
                    byte zero_page_addr = fetch(ticks, bus);
                    zero_page_addr = zeropage_bug(zero_page_addr + Y);
                    break;
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
                    zero_page_addr = zeropage_bug(zero_page_addr + X);
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
                    zero_page_addr = zeropage_bug(zero_page_addr + X);
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
                    zero_page_addr = zeropage_bug(zero_page_addr + Y);
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
                    zero_page_addr = zeropage_bug(zero_page_addr + X);
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
                    byte PS = processorstatus();
                    WriteByte(ticks, SP + 0x100, PS, bus);
                    SP--;
                    break;
                }
                case INS_PLA: {
                    A = ReadByte(ticks, SP + 0x100, bus);
                    LDSetStatusFlags(A);
                    SP++;
                    break;
                }
                case INS_PLP: {
                    byte PS = ReadByte(ticks, SP + 0x100, bus);
                    N = (PS >> 7) & 1;
                    V = (PS >> 6) & 1;
                    B = (PS >> 4) & 1;
                    D = (PS >> 3) & 1;
                    I = (PS >> 2) & 1;
                    Z = (PS >> 1) & 1;
                    C = (PS >> 0) & 1;
                    SP++;
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

void setup_vram_test_pattern(Bus &bus) {
    const word vram_start = 0xB000;

    // 1. Fill the entire 4,000 byte screen with a repeating cycle of glyphs
    for (u32 i = 0; i < 4000; ++i) {
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
    //setup_vram_test_pattern(bus);
    
    // Run the tile graphics renderer
    //std::cout << "Rendering 80x50 pixel canvas..." << std::endl;
    //bus.render_screen();
    // Populate RAM
    //bus.ram[0x0024] = 0x10; 
    //bus.ram[0x0025] = 0x80;
    //bus.ram[0x8010] = 0x18;
    // Fire up the emulation pipeline
    cpu.reset(bus);
    //cpu.X = 0x04; // Set X register to 5 for the LDA ZPX test
    cpu.execute(4, bus); // Executes the LDA operation
    
    return 0;
}