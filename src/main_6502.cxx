// 6502 Emulator
// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Connor Hopley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <string>
#include <fstream>
#include <iostream>
#include <SDL2/SDL.h>
#include <mutex>


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
    static const u32 MAIN_ROM_SIZE = 1024 * 14; // 14KB of main program ROM
    static const u32 CHAR_ROM_SIZE = 1024 * 2; // 2KB of character generator ROM

    byte main_data[MAIN_ROM_SIZE];
    byte char_data[CHAR_ROM_SIZE];

    void init() {
        for (u32 i = 0; i < MAIN_ROM_SIZE; ++i) {
            main_data[i] = 0;
        }
        for (u32 i = 0; i < CHAR_ROM_SIZE; ++i) {
            char_data[i] = 0;
        }
    }

    bool load_main_from_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Main ROM Error: Could not open file " << filename << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        if (size != MAIN_ROM_SIZE) {
            std::cerr << "Main ROM Error: File size is " << size << " bytes, expected " << MAIN_ROM_SIZE << " bytes." << std::endl;
            return false;
        }

        file.seekg(0, std::ios::beg);
        if (file.read(reinterpret_cast<char*>(main_data), MAIN_ROM_SIZE)) {
            return true;
        }

        std::cerr << "Main ROM Error: Failed to read file data." << std::endl;
        return false;
    }

    bool load_char_from_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Character ROM Error: Could not open file " << filename << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        if (size != CHAR_ROM_SIZE) {
            std::cerr << "Character ROM Error: File size is " << size << " bytes, expected " << CHAR_ROM_SIZE << " bytes." << std::endl;
            return false;
        }

        file.seekg(0, std::ios::beg);
        if (file.read(reinterpret_cast<char*>(char_data), CHAR_ROM_SIZE)) {
            return true;
        }

        std::cerr << "Character ROM Error: Failed to read file data." << std::endl;
        return false;
    }
};

struct Vram {
    static const u32 MAX_VRAM = 1024 * 4; // 4KB of VRAM
    std::atomic<byte> data[MAX_VRAM];
    void init() {
        for (u32 i = 0; i < MAX_VRAM; ++i) {
            data[i].store(0, std::memory_order_relaxed);
        }
    };

    byte read(u32 addr) const {
        return data[addr].load(std::memory_order_relaxed);
    }

    void write(u32 addr, byte value) {
        data[addr].store(value, std::memory_order_relaxed);
    }
};

struct Io {
    static const u32 MAX_IO = 1024 * 4; // 4KB of I/O space
    byte data[MAX_IO];
    void init() {
        for (u32 i = 0; i < MAX_IO; ++i) {
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
struct timer {

    std::atomic<bool> running{false};
    std::atomic<bool> irq_pending{false};

    std::thread timer_thread;

    void start(std::function<void()> callback, int interval_ms)
    {
        if (timer_thread.joinable()) {
            timer_thread.join();
        }

        running.store(true);
        irq_pending.store(false);

        timer_thread = std::thread(
            [this, callback, interval_ms]() {

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(interval_ms)
                );

                if (running.load()) {
                    callback();
                    running.store(false);
                }
            }
        );
    }

    void stop()
    {
        running.store(false);

        if (timer_thread.joinable()) {
            timer_thread.join();
        }
    }
};
struct KeyboardFIFO {
    static constexpr u32 SIZE = 32;

    byte data[SIZE];
    u32 head = 0;
    u32 tail = 0;
    u32 count = 0;

    mutable std::mutex mutex;

    bool push(byte value)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (count >= SIZE) {
            // FIFO full: discard the new key
            return false;
        }

        data[tail] = value;
        tail = (tail + 1) % SIZE;
        count++;

        return true;
    }

    byte pop()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (count == 0) {
            return 0;
        }

        byte value = data[head];

        head = (head + 1) % SIZE;
        count--;

        return value;
    }

    bool available() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return count != 0;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);

        head = 0;
        tail = 0;
        count = 0;
    }
};

// Unified Memory System that translates addresses automatically
struct Bus {
    Mem ram;
    Rom rom;
    Vram vram;
    Io io;
    mutable timer system_timer;
    mutable KeyboardFIFO keyboard;
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

        // Map 0xC000-0xFFFF as ROM. The character ROM occupies the first 2KB of this window,
        // while the remaining bytes are used for the main program ROM.
        for (int page = 12; page < 16; ++page) {
            read_map[page] = junk_page;
            write_map[page] = junk_page;
        }
    }

    // Zero conditional branches! Just bit shifts and pointer math.
    inline byte read(word address) const {
        if (address >= 0xC000 && address < 0xC800) {
            return rom.char_data[address - 0xC000];
        }
        if (address >= 0xC800 && address <= 0xFFFF) {
            return rom.main_data[address - 0xC800];
        }

        u32 page = address >> 12;         // Top 4 bits get the page index (0-15)
        u32 offset = address & 0x0FFF;    // Bottom 12 bits get the byte within that 4KB page
        if (page == 11) {
            return vram.read(offset);
        }
        if (page == 10) {
            switch (offset) {
                case 0x0000:
                    // $A000: keyboard FIFO data register.
                    // Reading removes one byte from the FIFO.
                    return keyboard.pop();

                case 0x0001:
                    // $A001: keyboard FIFO status.
                    // Bit 0 = data available.
                    return keyboard.available() ? 0x01 : 0x00;
                default:
                    return io.data[offset];
            }
        }
        return read_map[page][offset];
    }

    inline void write(word address, byte value) {
        if (address >= 0xC000 && address <= 0xFFFF) {
            // ROM area is read-only; discard writes to a dead buffer.
            junk_page[0] = value;
            return;
        }

        u32 page = address >> 12;
        u32 offset = address & 0x0FFF;
        if (page == 11) {
            vram.write(offset, value);
            return;
        }
        if (page == 10) {
            switch(offset) {
                case 0x0002:
                    // $A002: timer control register.
                    // Bit 0 = timer enabled.
                    if (value & 0x01) {
                        
                            system_timer.start([this]() {
                                system_timer.irq_pending.store(true);
                            }, read(0xA003) | (read(0xA004) << 8)); // 1 second interval
                        
                    } else {
                        system_timer.stop();
                        system_timer.irq_pending.store(false);
                    }
                    break;

                default:
                    io.data[offset] = value;
                    break;

            }
        }
        write_map[page][offset] = value;
    }

    void render_screen(std::atomic<bool> &running) {
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

    bool window_open = true;
    SDL_Event event;
 
    // Window event loop
    while (window_open && running.load(std::memory_order_relaxed)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running.store(false, std::memory_order_relaxed);
                window_open = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                int keycode = event.key.keysym.sym;
                byte value = static_cast<byte>(keycode & 0xFF);

                if (!keyboard.push(value)) {
                    std::cout << "Keyboard FIFO full, key dropped" << std::endl;
                }
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
    timer& system_timer;
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
    CPU(timer& timer_ref)
    : system_timer(timer_ref)
    {
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
        INS_STA_INX = 0x81,
        INS_STA_ABS = 0x8D,
        INS_STA_ABX = 0x9D,
        INS_STA_ABY = 0x99,
        INS_STA_INY = 0x91,
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
    void service_irq(Bus &bus){
    // Hardware IRQ pushes the current PC.
    WriteByte(0x0100 + SP, (PC >> 8) & 0xFF, bus);
    SP--;

    WriteByte(0x0100 + SP, PC & 0xFF, bus);
    SP--;

    // Push processor status.
    //
    // Bit 5 is always 1.
    // B is CLEAR for a hardware IRQ.
    byte stack_P = processorstatus() & ~0x10;

    WriteByte(0x0100 + SP, stack_P, bus);
    SP--;

    // Disable further maskable IRQs.
    I = 1;

    // Acknowledge the timer interrupt.
    system_timer.irq_pending.store(false);

    // Fetch IRQ vector from $FFFE/$FFFF.
    byte target_low = ReadByte(0xFFFE, bus);
    byte target_high = ReadByte(0xFFFF, bus);

    PC = target_low | ((word)target_high << 8);
}
    void execute_instruction(Bus & bus) {
        if (system_timer.irq_pending.load() && !I) {
            service_irq(bus);
            return;
        }
        word instruction_pc = PC;
        byte INS = fetch(bus);
        //printf("$%04X  %02X\n", instruction_pc, INS);
        switch (INS) {
                case INS_LDA_IMM: {
                    byte value = fetch(bus);
                    A = value;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ZP: {
                    byte zero_page_addr = fetch(bus);
                    A = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    A = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    A = ReadByte(addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABX: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    A = ReadByte(addr + X, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_ABY: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    A = ReadByte(addr + Y, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    A = ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDA_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    A = ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_LDX_IMM: {
                    byte value = fetch(bus);
                    X = value;
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ZP: {
                    byte zero_page_addr = fetch(bus);
                    X = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ZPY: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + Y;
                    X = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    X = ReadByte(addr, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDX_ABY: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    X = ReadByte(addr + Y, bus);
                    LDSetStatusFlags(X);
                    break;
                }
                case INS_LDY_IMM: {
                    byte value = fetch(bus);
                    Y = value;
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ZP: {
                    byte zero_page_addr = fetch(bus);
                    Y = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ZPX: {     
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    Y = ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    Y = ReadByte(addr, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_LDY_ABX: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    Y = ReadByte(addr + X, bus);
                    LDSetStatusFlags(Y);
                    break;
                }
                case INS_STA_ZP: {
                    byte zero_page_addr = fetch(bus);
                    WriteByte(zero_page_addr, A, bus);
                    break;
                }
                case INS_STA_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    WriteByte(zero_page_addr, A, bus);
                    break;
                }
                case INS_STA_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    WriteByte(effective_addr, A, bus);
                    break;
                }
                case INS_STA_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    WriteByte(addr, A, bus);
                    break;
                }
                case INS_STA_ABX: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    WriteByte(addr + X, A, bus);
                    break;
                }
                case INS_STA_ABY: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    WriteByte(addr + Y, A, bus);
                    break;
                }
                case INS_STA_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    WriteByte(effective_addr, A, bus);
                    break;
                }
                case INS_STX_ZP: {
                    byte zero_page_addr = fetch(bus);
                    WriteByte(zero_page_addr, X, bus);
                    break;
                }
                case INS_STX_ZPY: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + Y;
                    WriteByte(zero_page_addr, X, bus);
                    break;
                }
                case INS_STX_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    WriteByte(addr, X, bus);
                    break;
                }
                case INS_STY_ZP: {
                    byte zero_page_addr = fetch(bus);
                    WriteByte(zero_page_addr, Y, bus);
                    break;
                }
                case INS_STY_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    WriteByte(zero_page_addr, Y, bus);
                    break;
                }
                case INS_STY_ABS: {
                    word addr = fetch(bus); // Low byte
                    addr |= ((word)fetch(bus)) << 8; // High byte
                    WriteByte(addr, Y, bus);
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
                    WriteByte(SP + 0x100, A, bus);
                    SP--;
                    break;
                }
                case INS_PHP: {
                    byte PS = processorstatus() | 0x10;
                    WriteByte(SP + 0x100, PS, bus);
                    SP--;
                    break;
                }
                case INS_PLA: {
                    SP++;
                    A = ReadByte(SP + 0x100, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_PLP: {
                    SP++;
                    byte PS = ReadByte(SP + 0x100, bus);
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
                    byte opr = fetch(bus);
                    A = A&opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ZP: {
                    byte zero_page_addr = fetch(bus);
                    A = A&ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    A = A&ReadByte(zero_page_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABS: {
                    word addr = wordfetch(bus);
                    A = A&ReadByte(addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    A = A&ReadByte(addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    A = A&ReadByte(addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    A = A&ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_AND_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    A = A&ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_IMM: {
                    byte opr = fetch(bus);
                    A = A^opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ZP: {
                    byte zero_page_addr = fetch(bus);
                    A = A^ReadByte(zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    A = A^ReadByte(zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABS: {
                    word addr = wordfetch(bus);
                    A = A^ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    A = A^ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    A = A^ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    A = A^ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_EOR_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    A = A^ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_IMM:{
                    byte opr = fetch(bus);
                    A = A|opr;
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ZP: {
                    byte zero_page_addr = fetch(bus);
                    A = A|ReadByte(zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr+X;
                    A = A|ReadByte(zero_page_addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABS: {
                    word addr = wordfetch(bus);
                    A = A|ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    A = A|ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    A = A|ReadByte(addr,bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    A = A|ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_ORA_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    A = A|ReadByte(effective_addr, bus);
                    LDSetStatusFlags(A);
                    break;
                }
                case INS_BIT_ZP: {
                    byte zero_page_addr = fetch(bus);
                    byte value = ReadByte(zero_page_addr,bus);
                    Z = ((A & value) == 0);
                    N = (value >> 7) & 1;
                    V = (value >> 6) & 1;
                    break;

                }
                case INS_BIT_ABS: {
                    word addr = wordfetch(bus);
                    byte value = ReadByte(addr,bus);
                    Z = ((A & value) == 0);
                    N = (value >> 7) & 1;
                    V = (value >> 6) & 1;
                    break;

                }
                case INS_ADC_IMM: {
                    byte opr = fetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(opr, false,bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(opr, false, bus);
                    }
                    break;
                }
                case INS_ADC_ZP: {
                    byte zero_page_addr = fetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(zero_page_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(zero_page_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(zero_page_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(zero_page_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ABS: {
                    word addr = wordfetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), false, bus);
                    }
                    break;

                }
                case INS_ADC_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(effective_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(effective_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_ADC_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(effective_addr,bus), false, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(effective_addr,bus), false, bus);
                    }
                    break;
                }
                case INS_SBC_IMM: {
                    byte opr = fetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(opr, true,bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(opr, true, bus);
                    }
                    break;
                }
                case INS_SBC_ZP: {
                    byte zero_page_addr = fetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(zero_page_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(zero_page_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(zero_page_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(zero_page_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ABS: {
                    word addr = wordfetch(bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), true, bus);
                    }
                    break;

                }
                case INS_SBC_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_INX: {
                    word effective_addr = indexed_indirect(fetch(bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(effective_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(effective_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_SBC_INY: {
                    word effective_addr = indirect_indexed(fetch(bus), bus);
                    if (D != 1) {
                        A = ADC_SBC_HEX(ReadByte(effective_addr,bus), true, bus);
                        LDSetStatusFlags(A);
                    }
                    else {
                        A = ADC_SBC_BCD(ReadByte(effective_addr,bus), true, bus);
                    }
                    break;
                }
                case INS_CMP_IMM: {
                    byte opr = fetch(bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ZP: {
                    byte zero_page_addr = fetch(bus);
                    byte opr = ReadByte(zero_page_addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte opr = ReadByte(zero_page_addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABS: {
                    word addr = wordfetch(bus);
                    byte opr = ReadByte(addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte opr = ReadByte(addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_ABY: {
                    word addr = wordfetch(bus);
                    addr = addr + Y;
                    byte opr = ReadByte(addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_INX: {
                    word addr = indexed_indirect(fetch(bus), bus);
                    byte opr = ReadByte(addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CMP_INY: {
                    word addr = indirect_indexed(fetch(bus), bus);
                    byte opr = ReadByte(addr,bus);
                    byte result = A - opr;
                    C = (A >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_IMM: {
                    byte opr = fetch(bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_ZP: {
                    byte zero_page_addr = fetch(bus);
                    byte opr = ReadByte(zero_page_addr,bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPX_ABS: {
                    word addr = wordfetch(bus);
                    byte opr = ReadByte(addr,bus);
                    byte result = X - opr;
                    C = (X >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_IMM: {
                    byte opr = fetch(bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_ZP: {
                    byte zero_page_addr = fetch(bus);
                    byte opr = ReadByte(zero_page_addr,bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_CPY_ABS: {
                    word addr = wordfetch(bus);
                    byte opr = ReadByte(addr,bus);
                    byte result = Y - opr;
                    C = (Y >= opr) ? 1 : 0;
                    LDSetStatusFlags(result);
                    break;
                }
                case INS_INC_ZP: {
                    byte zero_page_addr = fetch(bus);
                    byte inc_tmp = ReadByte(zero_page_addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(zero_page_addr,inc_tmp,bus);
                    break;
                }
                case INS_INC_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte inc_tmp = ReadByte(zero_page_addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(zero_page_addr, inc_tmp, bus);
                    break;
                }
                case INS_INC_ABS: {
                    word addr = wordfetch(bus);
                    byte inc_tmp = ReadByte(addr, bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(addr, inc_tmp, bus);
                    break;
                }
                case INS_INC_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte inc_tmp = ReadByte(addr,bus);
                    inc_tmp++;
                    LDSetStatusFlags(inc_tmp);
                    WriteByte(addr, inc_tmp, bus);
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
                    byte zero_page_addr = fetch(bus);
                    byte dec_tmp = ReadByte(zero_page_addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(zero_page_addr,dec_tmp,bus);
                    break;
                }
                case INS_DEC_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte dec_tmp = ReadByte(zero_page_addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(zero_page_addr,dec_tmp,bus);
                    break;
                }
                case INS_DEC_ABS: {
                    word addr = wordfetch(bus);
                    byte dec_tmp = ReadByte(addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(addr, dec_tmp, bus);
                    break;
                }
                case INS_DEC_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte dec_tmp = ReadByte(addr, bus);
                    dec_tmp--;
                    LDSetStatusFlags(dec_tmp);
                    WriteByte(addr, dec_tmp, bus);
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
                    byte zero_page_addr = fetch(bus);
                    byte tmp = ReadByte(zero_page_addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_ASL_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(zero_page_addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_ASL_ABS: {
                    word addr = wordfetch(bus);
                    byte tmp = ReadByte(addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ASL_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte tmp = ReadByte(addr,bus);
                    C = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    WriteByte(addr,tmp,bus);
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
                    byte zero_page_addr = fetch(bus);
                    byte tmp = ReadByte(zero_page_addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_LSR_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(zero_page_addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(zero_page_addr,tmp,bus);
                    LDSetStatusFlags(tmp);    
                    break;
                }
                case INS_LSR_ABS: {
                    word addr = wordfetch(bus);
                    byte tmp = ReadByte(addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(addr,tmp,bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_LSR_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte tmp = ReadByte(addr,bus);
                    C = tmp & 1;
                    tmp = tmp >> 1;
                    WriteByte(addr,tmp,bus);
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
                    byte zero_page_addr = fetch(bus);
                    byte tmp = ReadByte(zero_page_addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(zero_page_addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ABS: {
                    word addr = wordfetch(bus);
                    byte tmp = ReadByte(addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROL_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte tmp = ReadByte(addr,bus);
                    byte tmpC = (tmp >> 7) & 1;
                    tmp = tmp << 1;
                    tmp |= C;
                    C = tmpC;
                    WriteByte(addr, tmp, bus);
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
                    byte zero_page_addr = fetch(bus);
                    byte tmp = ReadByte(zero_page_addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ZPX: {
                    byte zero_page_addr = fetch(bus);
                    zero_page_addr = zero_page_addr + X;
                    byte tmp = ReadByte(zero_page_addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(zero_page_addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ABS: {
                    word addr = wordfetch(bus);
                    byte tmp = ReadByte(addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_ROR_ABX: {
                    word addr = wordfetch(bus);
                    addr = addr + X;
                    byte tmp = ReadByte(addr, bus);
                    byte tmpC = tmp  & 1;
                    tmp = tmp >> 1;
                    tmp |= (C << 7);
                    C = tmpC;
                    WriteByte(addr, tmp, bus);
                    LDSetStatusFlags(tmp);
                    break;
                }
                case INS_JMP_ABS: {
                    word addr = wordfetch(bus);
                    PC = addr;
                    break;
                }
                case INS_JMP_IND: {
                    word addr = wordfetch(bus);
                    PC = ReadWordWithPageWrapBug(addr, bus);
                    break;
                }
                case INS_JSR_ABS: {
                    word target_addr = wordfetch(bus);
                    word return_addr = PC - 1;
                    WriteByte(0x0100 + SP, (return_addr >> 8) & 0xFF, bus);
                    SP--;
                    WriteByte(0x0100 + SP, return_addr & 0xFF, bus);
                    SP--;
                    PC = target_addr;
                    break;
                }
                case INS_RTS_IMP: {
                    SP++;
                    word return_addr_low = ReadByte(0x0100 + SP, bus);
                    SP++;
                    word return_addr_high = ReadByte(0x0100 + SP, bus);
                    
                    word return_addr = return_addr_low | (return_addr_high << 8);
                    PC = return_addr + 1;
                    break;
                }
                case INS_BCC: {
                    branch_relative(bus, C == 0);
                    break;
                }
                case INS_BCS: {
                    branch_relative(bus, C != 0);
                    break;
                }
                case INS_BEQ: {
                    branch_relative(bus, Z != 0);
                    break;
                }
                case INS_BMI: {
                    branch_relative(bus, N != 0);
                    break;
                }
                case INS_BNE: {
                    branch_relative(bus, Z == 0);
                    break;
                }
                case INS_BPL: {
                    branch_relative(bus, N == 0);
                    break;
                }
                case INS_BVC: {
                    branch_relative(bus, V == 0);
                    break;
                }
                case INS_BVS: {
                    branch_relative(bus, V != 0);
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
                    
                    WriteByte(0x0100 + SP, (PC >> 8) & 0xFF, bus);
                    SP--;

                    
                    WriteByte(0x0100 + SP, PC & 0xFF, bus);
                    SP--;
                    
                    byte stack_P = processorstatus() | 0x10 | 0x20; 
                    WriteByte(0x0100 + SP, stack_P, bus);
                    SP--;
                    I = 1;
                    byte target_low = ReadByte(0xFFFE, bus);
                    byte target_high = ReadByte(0xFFFF, bus);
    
                    PC = target_low | ((word)target_high << 8);
                    break;
                }
                case INS_NOP: {
                    break;
                }
                case INS_RTI: {
                    //Step up to the Status Register slot and pull it
                    SP++;
                    byte pulled_P = ReadByte(0x0100 + SP, bus);
    
                    // Unpack the pulled byte directly back into individual CPU flags.
                    C = (pulled_P >> 0) & 1;
                    Z = (pulled_P >> 1) & 1;
                    I = (pulled_P >> 2) & 1;
                    D = (pulled_P >> 3) & 1;
                    V = (pulled_P >> 6) & 1;
                    N = (pulled_P >> 7) & 1;

                    // Pull low PC
                    SP++;
                    word pc_low = ReadByte(0x0100 + SP, bus);

                    // Pull high PC
                    SP++;
                    word pc_high = ReadByte(0x0100 + SP, bus);

                    // Combine them into PC
                    PC = pc_low | (pc_high << 8);

                    break;
                }
                default:
                    printf("\n=== ILLEGAL OPCODE ===\n");
                    printf("PC after fetch : $%04X\n", PC - 1);
                    printf("Opcode         : $%02X\n", INS);
                    printf("A              : $%02X\n", A);
                    printf("X              : $%02X\n", X);
                    printf("Y              : $%02X\n", Y);
                    printf("SP             : $%02X\n", SP);
                    printf("P              : $%02X\n", processorstatus());

                    printf("Previous bytes : ");

                    for (int i = -4; i <= 4; ++i) {
                        word addr = static_cast<word>((PC - 1) + i);
                        printf("%02X ", bus.read(addr));
                    }

                    printf("\n");
                    exit(1);
        }
    }

    void step(Bus & bus) {
        execute_instruction(bus);
    }

    // Fetch the next byte
    byte fetch(Bus & bus) {
        byte instruction = bus.read(PC);
        PC++;
        return instruction;
    }
    word wordfetch(Bus & bus) {
        word addr = fetch(bus); // Low byte
        addr |= ((word)fetch(bus)) << 8; // High byte
        return addr;
    }

    // NMOS 6502 quirk: JMP ($xxFF) reads high byte from $xx00, not $(xx+1)00.
    word ReadWordWithPageWrapBug(word pointer, Bus & bus) {
        byte low = ReadByte(pointer, bus);
        word high_addr = (pointer & 0xFF00) | ((pointer + 1) & 0x00FF);
        byte high = ReadByte(high_addr, bus);
        return low | ((word)high << 8);
    }

    void branch_relative(Bus & bus, bool should_branch) {
        signed char offset = static_cast<signed char>(fetch(bus));
        if (should_branch) {
            PC = static_cast<word>(PC + offset);
        }
    }

    // ($nn, X)
    word indexed_indirect(word addr, Bus & bus) {
        byte base_zp_addr = (byte)(addr & 0xFF); 

        byte low_zp = (base_zp_addr + X) & 0xFF;
        byte effective_addr_low = ReadByte(low_zp, bus);

        byte high_zp = (base_zp_addr + X + 1) & 0xFF;
        byte effective_addr_high = ReadByte(high_zp, bus);

        return effective_addr_low | ((word)effective_addr_high << 8);
    }

    // ($nn),Y
    word indirect_indexed(word addr, Bus & bus) {
        byte base_zp_addr = (byte)(addr & 0xFF); // Ensure it's treated as a ZP byte

        byte effective_addr_low = ReadByte(base_zp_addr, bus);
        // Wrap high byte read to zero page if base_zp_addr is 0xFF
        byte effective_addr_high = ReadByte((base_zp_addr + 1) & 0xFF, bus); 
    
        word base_address = effective_addr_low | ((word)effective_addr_high << 8);
    
        // Add Y to the 16-bit base address here!
        return base_address + Y; 
    }
    byte ADC_SBC_HEX(byte opr, bool is_subtraction, Bus & bus) {
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
    byte ADC_SBC_BCD(byte opr, bool is_subtraction, Bus & bus) {
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

    byte ReadByte(word address, Bus & bus) {
        byte data = bus.read(address);
        return data;
    }

    void WriteByte(word address, byte data, Bus & bus) {
        bus.write(address, data);
    }
};

void setup_vram_test_pattern(Bus &bus) {
    const word vram_start = 0xB000;
    for (u32 i = 0; i < 80; ++i) {
        bus.write(vram_start + i, 0x20); // Fill the first row with spaces
    }
    // 1. Fill the entire 4,000 byte screen with a repeating cycle of glyphs
    for (u32 i = 80, x = 0; i < 4000; ++i, ++x) {
        // This cycles character indices 0 through 63 repeatedly across the grid
        bus.write(vram_start + i, static_cast<byte>(x % 64));
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
    CPU cpu(bus.system_timer);
    std::atomic<bool> running(true);
    
    bus.init();

    if (!bus.rom.load_main_from_file("rom.bin")) {
        std::cerr << "Failed to load main ROM file." << std::endl;
        return 1;
    }

    if (!bus.rom.load_char_from_file("char_rom.bin")) {
        std::cerr << "Failed to load character ROM file." << std::endl;
        return 1;
    }

    byte low_byte = bus.read(0xFFFC);
    byte high_byte = bus.read(0xFFFD);

    std::cout << "--- ROM Diagnostics ---" << std::endl;
    std::cout << "Main ROM byte at 0xFFFC (index 0x3FFC): 0x" << std::hex << (int)low_byte << std::endl;
    std::cout << "Main ROM byte at 0xFFFD (index 0x3FFD): 0x" << std::hex << (int)high_byte << std::endl;
    std::cout << "Character ROM byte at 0xC000: 0x" << std::hex << (int)bus.read(0xC000) << std::endl;
    // Fill VRAM with test indices
    //setup_vram_test_pattern(bus);
    
    // Run the tile graphics renderer
    std::cout << "Rendering 80x50 pixel canvas..." << std::endl;
    
    cpu.reset(bus);

    std::thread cpu_thread([&]() {
        while (running.load(std::memory_order_relaxed)) {
            cpu.step(bus);
        }
    });

    std::thread render_thread([&]() {
        bus.render_screen(running);
    });

    render_thread.join();
    running.store(false, std::memory_order_relaxed);
    cpu_thread.join();
    
    return 0;
}
