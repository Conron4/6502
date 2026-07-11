#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Pull emulator implementation into this test translation unit.
#define main emulator_entrypoint_for_tests
#include "main_6502.cxx"
#undef main

struct TestContext {
    int passed = 0;
    int failed = 0;

    void check(bool condition, const std::string& name, const std::string& detail = "") {
        if (condition) {
            passed++;
            return;
        }
        failed++;
        std::cerr << "[FAIL] " << name;
        if (!detail.empty()) {
            std::cerr << " :: " << detail;
        }
        std::cerr << std::endl;
    }

    template <typename T, typename U>
    void check_eq(const T& actual, const U& expected, const std::string& name, const std::string& detail = "") {
        if (actual == expected) {
            passed++;
            return;
        }
        failed++;
        std::ostringstream oss;
        oss << std::hex << std::showbase
            << "expected=" << static_cast<unsigned int>(expected)
            << " actual=" << static_cast<unsigned int>(actual);
        if (!detail.empty()) {
            oss << " " << detail;
        }
        std::cerr << "[FAIL] " << name << " :: " << oss.str() << std::endl;
    }
};

static constexpr word TEST_PC = 0x0200;

void fill_nops(Bus& bus, word start = 0x0000, word end = 0xAFFF) {
    for (u32 addr = start; addr <= end; ++addr) {
        bus.write(static_cast<word>(addr), CPU::INS_NOP);
    }
}

void write_word(Bus& bus, word addr, word value) {
    bus.write(addr, static_cast<byte>(value & 0x00FF));
    bus.write(static_cast<word>(addr + 1), static_cast<byte>((value >> 8) & 0x00FF));
}

void setup_cpu_bus(CPU& cpu, Bus& bus) {
    bus.init();
    fill_nops(bus);
    cpu.PC = TEST_PC;
    cpu.SP = 0xFF;
    cpu.A = cpu.X = cpu.Y = 0;
    cpu.C = cpu.Z = cpu.I = cpu.D = cpu.B = cpu.V = cpu.N = 0;
}

void run_ticks(CPU& cpu, Bus& bus, u32 ticks = 40) {
    cpu.execute(ticks, bus);
}

word place_abs_operand(Bus& bus, word pc, word address) {
    bus.write(static_cast<word>(pc + 1), static_cast<byte>(address & 0xFF));
    bus.write(static_cast<word>(pc + 2), static_cast<byte>((address >> 8) & 0xFF));
    return address;
}

void test_load_store(TestContext& t) {
    struct LoadCase {
        byte opcode;
        std::string name;
        byte init_reg;
        byte expected;
    };

    // LDA
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0;
        bus.write(TEST_PC, CPU::INS_LDA_IMM);
        bus.write(TEST_PC + 1, 0x42);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x42), "LDA_IMM");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x0044, 0x43);
        bus.write(TEST_PC, CPU::INS_LDA_ZP);
        bus.write(TEST_PC + 1, 0x44);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x43), "LDA_ZP");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x03;
        bus.write(0x0047, 0x44);
        bus.write(TEST_PC, CPU::INS_LDA_ZPX);
        bus.write(TEST_PC + 1, 0x44);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x44), "LDA_ZPX");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x1234, 0x45);
        bus.write(TEST_PC, CPU::INS_LDA_ABS);
        place_abs_operand(bus, TEST_PC, 0x1234);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x45), "LDA_ABS");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x05;
        bus.write(0x1239, 0x46);
        bus.write(TEST_PC, CPU::INS_LDA_ABX);
        place_abs_operand(bus, TEST_PC, 0x1234);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x46), "LDA_ABX");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x06;
        bus.write(0x123A, 0x47);
        bus.write(TEST_PC, CPU::INS_LDA_ABY);
        place_abs_operand(bus, TEST_PC, 0x1234);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x47), "LDA_ABY");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x04;
        bus.write(0x0024, 0x78);
        bus.write(0x0025, 0x56);
        bus.write(0x5678, 0x48);
        bus.write(TEST_PC, CPU::INS_LDA_INX);
        bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x48), "LDA_INX");
    }
    {
        CPU cpu;
        Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x05;
        bus.write(0x0030, 0x00);
        bus.write(0x0031, 0x40);
        bus.write(0x4005, 0x49);
        bus.write(TEST_PC, CPU::INS_LDA_INY);
        bus.write(TEST_PC + 1, 0x30);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x49), "LDA_INY");
    }

    // LDX
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_LDX_IMM);
        bus.write(TEST_PC + 1, 0x51);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x51), "LDX_IMM");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x0042, 0x52);
        bus.write(TEST_PC, CPU::INS_LDX_ZP);
        bus.write(TEST_PC + 1, 0x42);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x52), "LDX_ZP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x02;
        bus.write(0x0044, 0x53);
        bus.write(TEST_PC, CPU::INS_LDX_ZPY);
        bus.write(TEST_PC + 1, 0x42);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x53), "LDX_ZPY");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x2233, 0x54);
        bus.write(TEST_PC, CPU::INS_LDX_ABS);
        place_abs_operand(bus, TEST_PC, 0x2233);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x54), "LDX_ABS");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x02;
        bus.write(0x2235, 0x55);
        bus.write(TEST_PC, CPU::INS_LDX_ABY);
        place_abs_operand(bus, TEST_PC, 0x2233);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x55), "LDX_ABY");
    }

    // LDY
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_LDY_IMM);
        bus.write(TEST_PC + 1, 0x61);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x61), "LDY_IMM");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x0031, 0x62);
        bus.write(TEST_PC, CPU::INS_LDY_ZP);
        bus.write(TEST_PC + 1, 0x31);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x62), "LDY_ZP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x03;
        bus.write(0x0034, 0x63);
        bus.write(TEST_PC, CPU::INS_LDY_ZPX);
        bus.write(TEST_PC + 1, 0x31);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x63), "LDY_ZPX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        bus.write(0x3344, 0x64);
        bus.write(TEST_PC, CPU::INS_LDY_ABS);
        place_abs_operand(bus, TEST_PC, 0x3344);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x64), "LDY_ABS");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x02;
        bus.write(0x3346, 0x65);
        bus.write(TEST_PC, CPU::INS_LDY_ABX);
        place_abs_operand(bus, TEST_PC, 0x3344);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x65), "LDY_ABX");
    }

    // STA/STX/STY
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x71;
        bus.write(TEST_PC, CPU::INS_STA_ZP);
        bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0020), static_cast<byte>(0x71), "STA_ZP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x72; cpu.X = 0x02;
        bus.write(TEST_PC, CPU::INS_STA_ZPX);
        bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0022), static_cast<byte>(0x72), "STA_ZPX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x73;
        bus.write(TEST_PC, CPU::INS_STA_ABS);
        place_abs_operand(bus, TEST_PC, 0x2400);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x2400), static_cast<byte>(0x73), "STA_ABS");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x74; cpu.X = 0x03;
        bus.write(TEST_PC, CPU::INS_STA_ABX);
        place_abs_operand(bus, TEST_PC, 0x2400);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x2403), static_cast<byte>(0x74), "STA_ABX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x75; cpu.Y = 0x04;
        bus.write(TEST_PC, CPU::INS_STA_ABY);
        place_abs_operand(bus, TEST_PC, 0x2400);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x2404), static_cast<byte>(0x75), "STA_ABY");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x76;
        bus.write(TEST_PC, CPU::INS_STX_ZP);
        bus.write(TEST_PC + 1, 0x21);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0021), static_cast<byte>(0x76), "STX_ZP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x77; cpu.Y = 0x03;
        bus.write(TEST_PC, CPU::INS_STX_ZPY);
        bus.write(TEST_PC + 1, 0x21);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0024), static_cast<byte>(0x77), "STX_ZPY");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x78;
        bus.write(TEST_PC, CPU::INS_STX_ABS);
        place_abs_operand(bus, TEST_PC, 0x2500);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x2500), static_cast<byte>(0x78), "STX_ABS");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x79;
        bus.write(TEST_PC, CPU::INS_STY_ZP);
        bus.write(TEST_PC + 1, 0x22);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0022), static_cast<byte>(0x79), "STY_ZP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x7A; cpu.X = 0x02;
        bus.write(TEST_PC, CPU::INS_STY_ZPX);
        bus.write(TEST_PC + 1, 0x22);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x0024), static_cast<byte>(0x7A), "STY_ZPX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x7B;
        bus.write(TEST_PC, CPU::INS_STY_ABS);
        place_abs_operand(bus, TEST_PC, 0x2600);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x2600), static_cast<byte>(0x7B), "STY_ABS");
    }
}

void test_transfer_stack(TestContext& t) {
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x80;
        bus.write(TEST_PC, CPU::INS_TAX);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x80), "TAX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x81;
        bus.write(TEST_PC, CPU::INS_TAY);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Y, static_cast<byte>(0x81), "TAY");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x82;
        bus.write(TEST_PC, CPU::INS_TXA);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x82), "TXA");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.Y = 0x83;
        bus.write(TEST_PC, CPU::INS_TYA);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x83), "TYA");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.SP = 0x84;
        bus.write(TEST_PC, CPU::INS_TSX);
        run_ticks(cpu, bus);
        t.check_eq(cpu.X, static_cast<byte>(0x84), "TSX");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.X = 0x85;
        bus.write(TEST_PC, CPU::INS_TXS);
        run_ticks(cpu, bus);
        t.check_eq(cpu.SP, static_cast<byte>(0x85), "TXS");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.A = 0x86;
        bus.write(TEST_PC, CPU::INS_PHA);
        run_ticks(cpu, bus);
        t.check_eq(bus.read(0x01FF), static_cast<byte>(0x86), "PHA write");
        t.check_eq(cpu.SP, static_cast<byte>(0xFE), "PHA SP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.N = 1; cpu.V = 1; cpu.D = 1; cpu.I = 1; cpu.Z = 1; cpu.C = 1;
        bus.write(TEST_PC, CPU::INS_PHP);
        run_ticks(cpu, bus);
        t.check((bus.read(0x01FF) & 0x10) != 0, "PHP B flag pushed");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.SP = 0xFE;
        bus.write(0x01FF, 0x91);
        bus.write(TEST_PC, CPU::INS_PLA);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x91), "PLA value");
        t.check_eq(cpu.SP, static_cast<byte>(0xFF), "PLA SP");
    }
    {
        CPU cpu; Bus bus;
        setup_cpu_bus(cpu, bus);
        cpu.SP = 0xFE;
        bus.write(0x01FF, 0b11001101);
        bus.write(TEST_PC, CPU::INS_PLP);
        run_ticks(cpu, bus);
        t.check_eq(cpu.N, static_cast<byte>(1), "PLP N");
        t.check_eq(cpu.V, static_cast<byte>(1), "PLP V");
        t.check_eq(cpu.D, static_cast<byte>(1), "PLP D");
        t.check_eq(cpu.I, static_cast<byte>(1), "PLP I");
        t.check_eq(cpu.Z, static_cast<byte>(0), "PLP Z");
        t.check_eq(cpu.C, static_cast<byte>(1), "PLP C");
    }
}

void test_logical(TestContext& t) {
    // AND
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xF0;
        bus.write(TEST_PC, CPU::INS_AND_IMM); bus.write(TEST_PC + 1, 0x0F);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x00), "AND_IMM");
        t.check_eq(cpu.Z, static_cast<byte>(1), "AND_IMM Z");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; bus.write(0x0010, 0x0F);
        bus.write(TEST_PC, CPU::INS_AND_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0A), "AND_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xFF; cpu.X = 0x02; bus.write(0x0012, 0xF0);
        bus.write(TEST_PC, CPU::INS_AND_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xF0), "AND_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0F; bus.write(0x3000, 0xF3);
        bus.write(TEST_PC, CPU::INS_AND_ABS); place_abs_operand(bus, TEST_PC, 0x3000);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x03), "AND_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xF3; cpu.X = 1; bus.write(0x3001, 0x0F);
        bus.write(TEST_PC, CPU::INS_AND_ABX); place_abs_operand(bus, TEST_PC, 0x3000);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x03), "AND_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xF3; cpu.Y = 1; bus.write(0x3001, 0x03);
        bus.write(TEST_PC, CPU::INS_AND_ABY); place_abs_operand(bus, TEST_PC, 0x3000);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x03), "AND_ABY");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xFF; cpu.X = 0x04;
        bus.write(0x0024, 0x34); bus.write(0x0025, 0x12); bus.write(0x1234, 0x0C);
        bus.write(TEST_PC, CPU::INS_AND_INX); bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0C), "AND_INX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xFF; cpu.Y = 0x03;
        bus.write(0x0030, 0x00); bus.write(0x0031, 0x20); bus.write(0x2003, 0x0D);
        bus.write(TEST_PC, CPU::INS_AND_INY); bus.write(TEST_PC + 1, 0x30);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0D), "AND_INY");
    }

    // EOR
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA;
        bus.write(TEST_PC, CPU::INS_EOR_IMM); bus.write(TEST_PC + 1, 0xFF);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x55), "EOR_IMM");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; bus.write(0x0010, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.X = 0x01; bus.write(0x0011, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; bus.write(0x3010, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_ABS); place_abs_operand(bus, TEST_PC, 0x3010);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.X = 2; bus.write(0x3012, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_ABX); place_abs_operand(bus, TEST_PC, 0x3010);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.Y = 2; bus.write(0x3012, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_ABY); place_abs_operand(bus, TEST_PC, 0x3010);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_ABY");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.X = 1;
        bus.write(0x0021, 0x10); bus.write(0x0022, 0x40); bus.write(0x4010, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_INX); bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_INX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.Y = 1;
        bus.write(0x0030, 0x10); bus.write(0x0031, 0x40); bus.write(0x4011, 0x0F);
        bus.write(TEST_PC, CPU::INS_EOR_INY); bus.write(TEST_PC + 1, 0x30);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xA5), "EOR_INY");
    }

    // ORA
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A;
        bus.write(TEST_PC, CPU::INS_ORA_IMM); bus.write(TEST_PC + 1, 0xF0);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_IMM");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; bus.write(0x0010, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; cpu.X = 1; bus.write(0x0011, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; bus.write(0x3020, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_ABS); place_abs_operand(bus, TEST_PC, 0x3020);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; cpu.X = 1; bus.write(0x3021, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_ABX); place_abs_operand(bus, TEST_PC, 0x3020);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; cpu.Y = 1; bus.write(0x3021, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_ABY); place_abs_operand(bus, TEST_PC, 0x3020);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_ABY");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; cpu.X = 1;
        bus.write(0x0021, 0x20); bus.write(0x0022, 0x50); bus.write(0x5020, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_INX); bus.write(TEST_PC + 1, 0x20);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_INX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x0A; cpu.Y = 1;
        bus.write(0x0030, 0x20); bus.write(0x0031, 0x50); bus.write(0x5021, 0xF0);
        bus.write(TEST_PC, CPU::INS_ORA_INY); bus.write(TEST_PC + 1, 0x30);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0xFA), "ORA_INY");
    }

    // BIT
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0b01000000;
        bus.write(0x0010, 0b11000000);
        bus.write(TEST_PC, CPU::INS_BIT_ZP);
        bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Z, static_cast<byte>(0), "BIT_ZP Z");
        t.check_eq(cpu.N, static_cast<byte>(1), "BIT_ZP N");
        t.check_eq(cpu.V, static_cast<byte>(1), "BIT_ZP V");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x00;
        bus.write(0x3333, 0b01000000);
        bus.write(TEST_PC, CPU::INS_BIT_ABS);
        place_abs_operand(bus, TEST_PC, 0x3333);
        run_ticks(cpu, bus);
        t.check_eq(cpu.Z, static_cast<byte>(1), "BIT_ABS Z");
        t.check_eq(cpu.N, static_cast<byte>(0), "BIT_ABS N");
        t.check_eq(cpu.V, static_cast<byte>(1), "BIT_ABS V");
    }
}

void test_arithmetic_compare(TestContext& t) {
    auto adc_case = [&](byte opcode, const std::string& name, byte operand, byte expected, bool carry_in = false, byte x = 0, byte y = 0) {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = carry_in ? 1 : 0; cpu.X = x; cpu.Y = y;
        bus.write(TEST_PC, opcode);

        switch (opcode) {
            case CPU::INS_ADC_IMM:
            case CPU::INS_SBC_IMM:
                bus.write(TEST_PC + 1, operand);
                break;
            case CPU::INS_ADC_ZP:
            case CPU::INS_SBC_ZP:
                bus.write(TEST_PC + 1, 0x20); bus.write(0x0020, operand);
                break;
            case CPU::INS_ADC_ZPX:
            case CPU::INS_SBC_ZPX:
                bus.write(TEST_PC + 1, 0x20); bus.write(static_cast<word>(0x0020 + x), operand);
                break;
            case CPU::INS_ADC_ABS:
            case CPU::INS_SBC_ABS:
                place_abs_operand(bus, TEST_PC, 0x4440); bus.write(0x4440, operand);
                break;
            case CPU::INS_ADC_ABX:
            case CPU::INS_SBC_ABX:
                place_abs_operand(bus, TEST_PC, 0x4440); bus.write(static_cast<word>(0x4440 + x), operand);
                break;
            case CPU::INS_ADC_ABY:
            case CPU::INS_SBC_ABY:
                place_abs_operand(bus, TEST_PC, 0x4440); bus.write(static_cast<word>(0x4440 + y), operand);
                break;
            case CPU::INS_ADC_INX:
            case CPU::INS_SBC_INX:
                bus.write(TEST_PC + 1, 0x30);
                bus.write(static_cast<word>(0x0030 + x), 0x40);
                bus.write(static_cast<word>(0x0031 + x), 0x44);
                bus.write(0x4440, operand);
                break;
            case CPU::INS_ADC_INY:
            case CPU::INS_SBC_INY:
                bus.write(TEST_PC + 1, 0x30);
                bus.write(0x0030, 0x40);
                bus.write(0x0031, 0x44);
                bus.write(static_cast<word>(0x4440 + y), operand);
                break;
            default:
                break;
        }

        run_ticks(cpu, bus);
        t.check_eq(cpu.A, expected, name);
    };

    adc_case(CPU::INS_ADC_IMM, "ADC_IMM", 0x20, 0x30);
    adc_case(CPU::INS_ADC_ZP, "ADC_ZP", 0x20, 0x30);
    adc_case(CPU::INS_ADC_ZPX, "ADC_ZPX", 0x20, 0x30, false, 0x02);
    adc_case(CPU::INS_ADC_ABS, "ADC_ABS", 0x20, 0x30);
    adc_case(CPU::INS_ADC_ABX, "ADC_ABX", 0x20, 0x30, false, 0x01);
    adc_case(CPU::INS_ADC_ABY, "ADC_ABY", 0x20, 0x30, false, 0x00, 0x01);
    adc_case(CPU::INS_ADC_INX, "ADC_INX", 0x20, 0x30, false, 0x01);
    adc_case(CPU::INS_ADC_INY, "ADC_INY", 0x20, 0x30, false, 0x00, 0x01);

    // SBC with C=1 means A - operand.
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1;
        bus.write(TEST_PC, CPU::INS_SBC_IMM); bus.write(TEST_PC + 1, 0x01);
        run_ticks(cpu, bus);
        t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_IMM");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1;
        bus.write(TEST_PC, CPU::INS_SBC_ZP); bus.write(TEST_PC + 1, 0x20); bus.write(0x0020, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1; cpu.X = 2;
        bus.write(TEST_PC, CPU::INS_SBC_ZPX); bus.write(TEST_PC + 1, 0x20); bus.write(0x0022, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1;
        bus.write(TEST_PC, CPU::INS_SBC_ABS); place_abs_operand(bus, TEST_PC, 0x4100); bus.write(0x4100, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1; cpu.X = 1;
        bus.write(TEST_PC, CPU::INS_SBC_ABX); place_abs_operand(bus, TEST_PC, 0x4100); bus.write(0x4101, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1; cpu.Y = 1;
        bus.write(TEST_PC, CPU::INS_SBC_ABY); place_abs_operand(bus, TEST_PC, 0x4100); bus.write(0x4101, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_ABY");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1; cpu.X = 1;
        bus.write(TEST_PC, CPU::INS_SBC_INX); bus.write(TEST_PC + 1, 0x30);
        bus.write(0x0031, 0x00); bus.write(0x0032, 0x41); bus.write(0x4100, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_INX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x10; cpu.C = 1; cpu.Y = 1;
        bus.write(TEST_PC, CPU::INS_SBC_INY); bus.write(TEST_PC + 1, 0x30);
        bus.write(0x0030, 0x00); bus.write(0x0031, 0x41); bus.write(0x4101, 0x01);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x0F), "SBC_INY");
    }

    auto cmpA = [&](byte opcode, const std::string& name) {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x20; cpu.X = 1; cpu.Y = 1;
        bus.write(TEST_PC, opcode);
        switch (opcode) {
            case CPU::INS_CMP_IMM: bus.write(TEST_PC + 1, 0x10); break;
            case CPU::INS_CMP_ZP: bus.write(TEST_PC + 1, 0x20); bus.write(0x0020, 0x10); break;
            case CPU::INS_CMP_ZPX: bus.write(TEST_PC + 1, 0x20); bus.write(0x0021, 0x10); break;
            case CPU::INS_CMP_ABS: place_abs_operand(bus, TEST_PC, 0x4200); bus.write(0x4200, 0x10); break;
            case CPU::INS_CMP_ABX: place_abs_operand(bus, TEST_PC, 0x4200); bus.write(0x4201, 0x10); break;
            case CPU::INS_CMP_ABY: place_abs_operand(bus, TEST_PC, 0x4200); bus.write(0x4201, 0x10); break;
            case CPU::INS_CMP_INX:
                bus.write(TEST_PC + 1, 0x30); bus.write(0x0031, 0x00); bus.write(0x0032, 0x42); bus.write(0x4200, 0x10); break;
            case CPU::INS_CMP_INY:
                bus.write(TEST_PC + 1, 0x30); bus.write(0x0030, 0x00); bus.write(0x0031, 0x42); bus.write(0x4201, 0x10); break;
            default: break;
        }
        run_ticks(cpu, bus);
        t.check_eq(cpu.C, static_cast<byte>(1), name + " C");
        t.check_eq(cpu.Z, static_cast<byte>(0), name + " Z");
    };

    cmpA(CPU::INS_CMP_IMM, "CMP_IMM");
    cmpA(CPU::INS_CMP_ZP, "CMP_ZP");
    cmpA(CPU::INS_CMP_ZPX, "CMP_ZPX");
    cmpA(CPU::INS_CMP_ABS, "CMP_ABS");
    cmpA(CPU::INS_CMP_ABX, "CMP_ABX");
    cmpA(CPU::INS_CMP_ABY, "CMP_ABY");
    cmpA(CPU::INS_CMP_INX, "CMP_INX");
    cmpA(CPU::INS_CMP_INY, "CMP_INY");

    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 0x20;
        bus.write(TEST_PC, CPU::INS_CPX_IMM); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPX_IMM");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 0x20;
        bus.write(TEST_PC, CPU::INS_CPX_ZP); bus.write(TEST_PC + 1, 0x40); bus.write(0x0040, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPX_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 0x20;
        bus.write(TEST_PC, CPU::INS_CPX_ABS); place_abs_operand(bus, TEST_PC, 0x4300); bus.write(0x4300, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPX_ABS");
    }

    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.Y = 0x20;
        bus.write(TEST_PC, CPU::INS_CPY_IMM); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPY_IMM");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.Y = 0x20;
        bus.write(TEST_PC, CPU::INS_CPY_ZP); bus.write(TEST_PC + 1, 0x40); bus.write(0x0040, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPY_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.Y = 0x20;
        bus.write(TEST_PC, CPU::INS_CPY_ABS); place_abs_operand(bus, TEST_PC, 0x4300); bus.write(0x4300, 0x10);
        run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "CPY_ABS");
    }
}

void test_inc_dec_shifts(TestContext& t) {
    // INC/INX/INY
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x0010, 0x0A); bus.write(TEST_PC, CPU::INS_INC_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x0B), "INC_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x0011, 0x0A); bus.write(TEST_PC, CPU::INS_INC_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x0B), "INC_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x4400, 0x0A); bus.write(TEST_PC, CPU::INS_INC_ABS); place_abs_operand(bus, TEST_PC, 0x4400);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4400), static_cast<byte>(0x0B), "INC_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x4401, 0x0A); bus.write(TEST_PC, CPU::INS_INC_ABX); place_abs_operand(bus, TEST_PC, 0x4400);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4401), static_cast<byte>(0x0B), "INC_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 0x0A; bus.write(TEST_PC, CPU::INS_INX);
        run_ticks(cpu, bus); t.check_eq(cpu.X, static_cast<byte>(0x0B), "INX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.Y = 0x0A; bus.write(TEST_PC, CPU::INS_INY);
        run_ticks(cpu, bus); t.check_eq(cpu.Y, static_cast<byte>(0x0B), "INY");
    }

    // DEC/DEX/DEY
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x0010, 0x0A); bus.write(TEST_PC, CPU::INS_DEC_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x09), "DEC_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x0011, 0x0A); bus.write(TEST_PC, CPU::INS_DEC_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x09), "DEC_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x4500, 0x0A); bus.write(TEST_PC, CPU::INS_DEC_ABS); place_abs_operand(bus, TEST_PC, 0x4500);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4500), static_cast<byte>(0x09), "DEC_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x4501, 0x0A); bus.write(TEST_PC, CPU::INS_DEC_ABX); place_abs_operand(bus, TEST_PC, 0x4500);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4501), static_cast<byte>(0x09), "DEC_ABX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 0x0A; bus.write(TEST_PC, CPU::INS_DEX);
        run_ticks(cpu, bus); t.check_eq(cpu.X, static_cast<byte>(0x09), "DEX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.Y = 0x0A; bus.write(TEST_PC, CPU::INS_DEY);
        run_ticks(cpu, bus); t.check_eq(cpu.Y, static_cast<byte>(0x09), "DEY");
    }

    // ASL
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x81; bus.write(TEST_PC, CPU::INS_ASL_ACC);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x02), "ASL_ACC"); t.check_eq(cpu.C, static_cast<byte>(1), "ASL_ACC C");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x0010, 0x81); bus.write(TEST_PC, CPU::INS_ASL_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x02), "ASL_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x0011, 0x81); bus.write(TEST_PC, CPU::INS_ASL_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x02), "ASL_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x4600, 0x81); bus.write(TEST_PC, CPU::INS_ASL_ABS); place_abs_operand(bus, TEST_PC, 0x4600);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4600), static_cast<byte>(0x02), "ASL_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x4601, 0x81); bus.write(TEST_PC, CPU::INS_ASL_ABX); place_abs_operand(bus, TEST_PC, 0x4600);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4601), static_cast<byte>(0x02), "ASL_ABX");
    }

    // LSR
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x03; bus.write(TEST_PC, CPU::INS_LSR_ACC);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x01), "LSR_ACC"); t.check_eq(cpu.C, static_cast<byte>(1), "LSR_ACC C");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x0010, 0x03); bus.write(TEST_PC, CPU::INS_LSR_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x01), "LSR_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x0011, 0x03); bus.write(TEST_PC, CPU::INS_LSR_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x01), "LSR_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(0x4700, 0x03); bus.write(TEST_PC, CPU::INS_LSR_ABS); place_abs_operand(bus, TEST_PC, 0x4700);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4700), static_cast<byte>(0x01), "LSR_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.X = 1; bus.write(0x4701, 0x03); bus.write(TEST_PC, CPU::INS_LSR_ABX); place_abs_operand(bus, TEST_PC, 0x4700);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4701), static_cast<byte>(0x01), "LSR_ABX");
    }

    // ROL
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x80; cpu.C = 1; bus.write(TEST_PC, CPU::INS_ROL_ACC);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x01), "ROL_ACC"); t.check_eq(cpu.C, static_cast<byte>(1), "ROL_ACC C");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; bus.write(0x0010, 0x80); bus.write(TEST_PC, CPU::INS_ROL_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x01), "ROL_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; cpu.X = 1; bus.write(0x0011, 0x80); bus.write(TEST_PC, CPU::INS_ROL_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x01), "ROL_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; bus.write(0x4800, 0x80); bus.write(TEST_PC, CPU::INS_ROL_ABS); place_abs_operand(bus, TEST_PC, 0x4800);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4800), static_cast<byte>(0x01), "ROL_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; cpu.X = 1; bus.write(0x4801, 0x80); bus.write(TEST_PC, CPU::INS_ROL_ABX); place_abs_operand(bus, TEST_PC, 0x4800);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4801), static_cast<byte>(0x01), "ROL_ABX");
    }

    // ROR
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0x01; cpu.C = 1; bus.write(TEST_PC, CPU::INS_ROR_ACC);
        run_ticks(cpu, bus); t.check_eq(cpu.A, static_cast<byte>(0x80), "ROR_ACC"); t.check_eq(cpu.C, static_cast<byte>(1), "ROR_ACC C");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; bus.write(0x0010, 0x01); bus.write(TEST_PC, CPU::INS_ROR_ZP); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0010), static_cast<byte>(0x80), "ROR_ZP");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; cpu.X = 1; bus.write(0x0011, 0x01); bus.write(TEST_PC, CPU::INS_ROR_ZPX); bus.write(TEST_PC + 1, 0x10);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x0011), static_cast<byte>(0x80), "ROR_ZPX");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; bus.write(0x4900, 0x01); bus.write(TEST_PC, CPU::INS_ROR_ABS); place_abs_operand(bus, TEST_PC, 0x4900);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4900), static_cast<byte>(0x80), "ROR_ABS");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; cpu.X = 1; bus.write(0x4901, 0x01); bus.write(TEST_PC, CPU::INS_ROR_ABX); place_abs_operand(bus, TEST_PC, 0x4900);
        run_ticks(cpu, bus); t.check_eq(bus.read(0x4901), static_cast<byte>(0x80), "ROR_ABX");
    }
}

void test_jumps_branches_status_system(TestContext& t) {
    // JMP ABS
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_JMP_ABS);
        place_abs_operand(bus, TEST_PC, 0x0300);
        bus.write(0x0300, CPU::INS_LDA_IMM);
        bus.write(0x0301, 0x9A);
        run_ticks(cpu, bus, 10);
        t.check_eq(cpu.A, static_cast<byte>(0x9A), "JMP_ABS");
    }

    // JMP IND and NMOS page-wrap bug behavior
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_JMP_IND);
        place_abs_operand(bus, TEST_PC, 0x10FF);
        // 6502 bug: high byte read from 0x1000, not 0x1100.
        bus.write(0x10FF, 0x00);
        bus.write(0x1000, 0x04);
        bus.write(0x0400, CPU::INS_LDA_IMM);
        bus.write(0x0401, 0x9B);
        run_ticks(cpu, bus, 15);
        t.check_eq(cpu.A, static_cast<byte>(0x9B), "JMP_IND page-wrap bug");
    }

    // JSR
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_JSR_ABS);
        place_abs_operand(bus, TEST_PC, 0x0350);
        bus.write(0x0350, CPU::INS_LDA_IMM);
        bus.write(0x0351, 0x9C);
        run_ticks(cpu, bus, 15);
        t.check_eq(cpu.SP, static_cast<byte>(0xFD), "JSR SP");
        t.check_eq(bus.read(0x01FF), static_cast<byte>(0x02), "JSR return high");
        t.check_eq(bus.read(0x01FE), static_cast<byte>(0x02), "JSR return low");
        t.check_eq(cpu.A, static_cast<byte>(0x9C), "JSR jumped");
    }

    // RTS
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.SP = 0xFD;
        bus.write(0x01FE, 0x00); // low
        bus.write(0x01FF, 0x04); // high -> returns to 0x0401
        bus.write(0x0401, CPU::INS_LDA_IMM);
        bus.write(0x0402, 0x9D);
        bus.write(TEST_PC, CPU::INS_RTS_IMP);
        run_ticks(cpu, bus, 20);
        t.check_eq(cpu.A, static_cast<byte>(0x9D), "RTS");
    }

    auto branch_test = [&](byte opcode, const std::string& name, bool set_condition) {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, opcode);
        bus.write(TEST_PC + 1, 0x02); // branch to the second LDA immediate
        bus.write(TEST_PC + 2, CPU::INS_LDA_IMM);
        bus.write(TEST_PC + 3, 0x10);
        bus.write(TEST_PC + 4, CPU::INS_LDA_IMM);
        bus.write(TEST_PC + 5, 0x20);

        if (opcode == CPU::INS_BCC) cpu.C = set_condition ? 0 : 1;
        if (opcode == CPU::INS_BCS) cpu.C = set_condition ? 1 : 0;
        if (opcode == CPU::INS_BEQ) cpu.Z = set_condition ? 1 : 0;
        if (opcode == CPU::INS_BMI) cpu.N = set_condition ? 1 : 0;
        if (opcode == CPU::INS_BNE) cpu.Z = set_condition ? 0 : 1;
        if (opcode == CPU::INS_BPL) cpu.N = set_condition ? 0 : 1;
        if (opcode == CPU::INS_BVC) cpu.V = set_condition ? 0 : 1;
        if (opcode == CPU::INS_BVS) cpu.V = set_condition ? 1 : 0;

        run_ticks(cpu, bus, 4);
        t.check_eq(cpu.A, static_cast<byte>(set_condition ? 0x20 : 0x10), name + (set_condition ? " taken" : " not-taken"));
    };

    branch_test(CPU::INS_BCC, "BCC", true);
    branch_test(CPU::INS_BCS, "BCS", true);
    branch_test(CPU::INS_BEQ, "BEQ", true);
    branch_test(CPU::INS_BMI, "BMI", true);
    branch_test(CPU::INS_BNE, "BNE", true);
    branch_test(CPU::INS_BPL, "BPL", true);
    branch_test(CPU::INS_BVC, "BVC", true);
    branch_test(CPU::INS_BVS, "BVS", true);

    branch_test(CPU::INS_BCC, "BCC", false);
    branch_test(CPU::INS_BCS, "BCS", false);
    branch_test(CPU::INS_BEQ, "BEQ", false);
    branch_test(CPU::INS_BMI, "BMI", false);
    branch_test(CPU::INS_BNE, "BNE", false);
    branch_test(CPU::INS_BPL, "BPL", false);
    branch_test(CPU::INS_BVC, "BVC", false);
    branch_test(CPU::INS_BVS, "BVS", false);

    // Status flags
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 1; bus.write(TEST_PC, CPU::INS_CLC); run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(0), "CLC");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.D = 1; bus.write(TEST_PC, CPU::INS_CLD); run_ticks(cpu, bus); t.check_eq(cpu.D, static_cast<byte>(0), "CLD");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.I = 1; bus.write(TEST_PC, CPU::INS_CLI); run_ticks(cpu, bus); t.check_eq(cpu.I, static_cast<byte>(0), "CLI");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.V = 1; bus.write(TEST_PC, CPU::INS_CLV); run_ticks(cpu, bus); t.check_eq(cpu.V, static_cast<byte>(0), "CLV");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.C = 0; bus.write(TEST_PC, CPU::INS_SEC); run_ticks(cpu, bus); t.check_eq(cpu.C, static_cast<byte>(1), "SEC");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.D = 0; bus.write(TEST_PC, CPU::INS_SED); run_ticks(cpu, bus); t.check_eq(cpu.D, static_cast<byte>(1), "SED");
    }
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.I = 0; bus.write(TEST_PC, CPU::INS_SEI); run_ticks(cpu, bus); t.check_eq(cpu.I, static_cast<byte>(1), "SEI");
    }

    // BRK
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        bus.write(TEST_PC, CPU::INS_BRK);
        bus.write(0xFFFE, 0x00);
        bus.write(0xFFFF, 0x06);
        run_ticks(cpu, bus, 20);
        t.check_eq(cpu.I, static_cast<byte>(1), "BRK sets I");
        t.check_eq(cpu.SP, static_cast<byte>(0xFC), "BRK SP");
        t.check((bus.read(0x01FD) & 0x10) != 0, "BRK pushes B in stack copy");
    }

    // NOP
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.A = 0xAA; cpu.X = 0xBB; cpu.Y = 0xCC;
        bus.write(TEST_PC, CPU::INS_NOP);
        run_ticks(cpu, bus, 5);
        t.check_eq(cpu.A, static_cast<byte>(0xAA), "NOP A unchanged");
        t.check_eq(cpu.X, static_cast<byte>(0xBB), "NOP X unchanged");
        t.check_eq(cpu.Y, static_cast<byte>(0xCC), "NOP Y unchanged");
    }

    // RTI
    {
        CPU cpu; Bus bus; setup_cpu_bus(cpu, bus);
        cpu.SP = 0xFC;
        bus.write(0x01FD, 0b11001101); // P
        bus.write(0x01FE, 0x50);       // low PC
        bus.write(0x01FF, 0x06);       // high PC => 0x0650
        bus.write(TEST_PC, CPU::INS_RTI);
        bus.write(0x0650, CPU::INS_LDA_IMM);
        bus.write(0x0651, 0xDA);
        run_ticks(cpu, bus, 20);
        t.check_eq(cpu.A, static_cast<byte>(0xDA), "RTI returns to PC");
        t.check_eq(cpu.C, static_cast<byte>(1), "RTI C");
        t.check_eq(cpu.V, static_cast<byte>(1), "RTI V");
        t.check_eq(cpu.N, static_cast<byte>(1), "RTI N");
    }
}

int main() {
    TestContext t;

    test_load_store(t);
    test_transfer_stack(t);
    test_logical(t);
    test_arithmetic_compare(t);
    test_inc_dec_shifts(t);
    test_jumps_branches_status_system(t);

    std::cout << "Opcode test summary: passed=" << t.passed << " failed=" << t.failed << std::endl;
    if (t.failed != 0) {
        return 1;
    }
    return 0;
}
