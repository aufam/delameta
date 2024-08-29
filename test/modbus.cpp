#include <delameta/modbus/modbus.h>
#include <gtest/gtest.h>

using namespace Project;
namespace modbus = delameta::modbus;

TEST(Modbus, coils) {
    static bool coils[4] = {1, 0, 0, 0};

    modbus::Modbus handler(0x0f);
    handler.CoilSetter(0x1001, [](bool value) { coils[0] = value; });
    handler.CoilSetter(0x1002, [](bool value) { coils[1] = value; });
    handler.CoilSetter(0x1003, [](bool value) { coils[2] = value; });
    handler.CoilSetter(0x1004, [](bool value) { coils[3] = value; });

    handler.CoilGetter(0x1001, []() { return coils[0]; });
    handler.CoilGetter(0x1002, []() { return coils[1]; });
    handler.CoilGetter(0x1003, []() { return coils[2]; });
    handler.CoilGetter(0x1004, []() { return coils[3]; });

    {
        std::vector<uint8_t> req;
        req.push_back(0x0f);
        req.push_back(modbus::FunctionCodeWriteSingleCoil);
        req.push_back(0x10);
        req.push_back(0x03);
        req.push_back(0xff);
        req.push_back(0x00);
        modbus::add_checksum(req);

        auto res = handler.execute(req).unwrap();
        EXPECT_EQ(req, res);
    } {
        std::vector<uint8_t> req;
        req.push_back(0x0f);
        req.push_back(modbus::FunctionCodeReadCoils);
        req.push_back(0x10);
        req.push_back(0x01);
        req.push_back(0x00);
        req.push_back(0x04);
        modbus::add_checksum(req);

        auto res = handler.execute(req).unwrap();
        EXPECT_EQ(res.size(), 6);
        EXPECT_EQ(res[0], 0x0f);
        EXPECT_EQ(res[1], modbus::FunctionCodeReadCoils);
        EXPECT_EQ(res[2], 0x01);
        EXPECT_EQ(res[3], 0b0101);
    }
}

TEST(Modbus, holding_registers) {
    static uint16_t holding_registers[4] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd};

    modbus::Modbus handler(0x0f);
    handler.HoldingRegisterSetter(0x2001, [](uint16_t value) { holding_registers[0] = value; });
    handler.HoldingRegisterSetter(0x2002, [](uint16_t value) { holding_registers[1] = value; });
    handler.HoldingRegisterSetter(0x2003, [](uint16_t value) { holding_registers[2] = value; });
    handler.HoldingRegisterSetter(0x2004, [](uint16_t value) { holding_registers[3] = value; });

    handler.HoldingRegisterGetter(0x2001, []() { return holding_registers[0]; });
    handler.HoldingRegisterGetter(0x2002, []() { return holding_registers[1]; });
    handler.HoldingRegisterGetter(0x2003, []() { return holding_registers[2]; });
    handler.HoldingRegisterGetter(0x2004, []() { return holding_registers[3]; });

    {
        std::vector<uint8_t> req;
        req.push_back(0x0f);
        req.push_back(modbus::FunctionCodeWriteSingleRegister);
        req.push_back(0x20);
        req.push_back(0x03);
        req.push_back(0x12);
        req.push_back(0x34);
        modbus::add_checksum(req);

        auto res = handler.execute(req).unwrap();
        EXPECT_EQ(req, res);
    } {
        std::vector<uint8_t> req;
        req.push_back(0x0f);
        req.push_back(modbus::FunctionCodeReadHoldingRegisters);
        req.push_back(0x20);
        req.push_back(0x01);
        req.push_back(0x00);
        req.push_back(0x04);
        modbus::add_checksum(req);

        auto res = handler.execute(req).unwrap();
        EXPECT_EQ(res.size(), 13);
        EXPECT_EQ(res[0], 0x0f);
        EXPECT_EQ(res[1], modbus::FunctionCodeReadHoldingRegisters);
        EXPECT_EQ(res[2], 0x08);
        EXPECT_EQ(res[3], 0xaa);
        EXPECT_EQ(res[4], 0xaa);
        EXPECT_EQ(res[5], 0xbb);
        EXPECT_EQ(res[6], 0xbb);
        EXPECT_EQ(res[7], 0x12);
        EXPECT_EQ(res[8], 0x34);
        EXPECT_EQ(res[9], 0xdd);
        EXPECT_EQ(res[10], 0xdd);
    }
}

TEST(Modbus, analog_inputs) {
    static uint16_t analog_inputs[4] = {0x0123, 0x4567, 0x89ab, 0xcdef};

    modbus::Modbus handler(0x0f);
    handler.AnalogInputGetter(0x3001, []() { return analog_inputs[0]; });
    handler.AnalogInputGetter(0x3002, []() { return analog_inputs[1]; });
    handler.AnalogInputGetter(0x3003, []() { return analog_inputs[2]; });
    handler.AnalogInputGetter(0x3004, []() { return analog_inputs[3]; });

    std::vector<uint8_t> req;
    req.push_back(0x0f);
    req.push_back(modbus::FunctionCodeReadInputRegisters);
    req.push_back(0x30);
    req.push_back(0x01);
    req.push_back(0x00);
    req.push_back(0x04);
    modbus::add_checksum(req);

    auto res = handler.execute(req).unwrap();
    EXPECT_EQ(res.size(), 13);
    EXPECT_EQ(res[0], 0x0f);
    EXPECT_EQ(res[1], modbus::FunctionCodeReadInputRegisters);
    EXPECT_EQ(res[2], 0x08);
    EXPECT_EQ(res[3], 0x01);
    EXPECT_EQ(res[4], 0x23);
    EXPECT_EQ(res[5], 0x45);
    EXPECT_EQ(res[6], 0x67);
    EXPECT_EQ(res[7], 0x89);
    EXPECT_EQ(res[8], 0xab);
    EXPECT_EQ(res[9], 0xcd);
    EXPECT_EQ(res[10], 0xef);
}