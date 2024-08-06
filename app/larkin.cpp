#include "delameta/http/server.h"
#include "delameta/modbus/rtu/client.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace Project;
using namespace delameta;
using namespace std::literals;
using delameta::modbus::rtu::Client;

static float power_of_ten(int exponent) {
    float result = 1.0;
    for (int i = 0; i < exponent; ++i) {
        result *= 10;
    }
    return result;
}

struct Larkin {
    static constexpr uint16_t RegisterAddress = 0x0023; 
    static constexpr uint16_t RegisterSize = 28;

    float phase_voltage_a;
    float phase_voltage_b;
    float phase_voltage_c;
    
    float line_voltage_ab;
    float line_voltage_bc;
    float line_voltage_ca;
    
    float phase_current_a;
    float phase_current_b;
    float phase_current_c;
    
    float phase_active_power_a;
    float phase_active_power_b;
    float phase_active_power_c;
    float total_active_power;

    float phase_reactive_power_a;
    float phase_reactive_power_b;
    float phase_reactive_power_c;
    float total_reactive_power;

    float phase_power_factor_a;
    float phase_power_factor_b;
    float phase_power_factor_c;
    float total_power_factor;

    float phase_apparent_power_a;
    float phase_apparent_power_b;
    float phase_apparent_power_c;
    float total_apparent_power;

    float frequency;

    void print();

    static http::Server::Result<Larkin> New(const std::vector<uint16_t>& data) {
        if (data.size() != RegisterSize) 
            return etl::Err(
                http::Server::Error{http::StatusInternalServerError, 
                "received size is not valid: " + std::to_string(data.size())
            });
        
        uint8_t dpt = (data[0] >> 8) & 0xff;
        uint8_t dct = (data[0] >> 0) & 0xff;
        uint8_t dpq = (data[1] >> 8) & 0xff;
        uint8_t sign = (data[1] >> 0) & 0xff;

        auto res = Larkin{
            .phase_voltage_a=data[2] * 1e-4f * power_of_ten(dpt),
            .phase_voltage_b=data[3] * 1e-4f * power_of_ten(dpt),
            .phase_voltage_c=data[4] * 1e-4f * power_of_ten(dpt),

            .line_voltage_ab=data[5] * 1e-4f * power_of_ten(dpt),
            .line_voltage_bc=data[6] * 1e-4f * power_of_ten(dpt),
            .line_voltage_ca=data[7] * 1e-4f * power_of_ten(dpt),

            .phase_current_a=data[8] * 1e-4f * power_of_ten(dct),
            .phase_current_b=data[9] * 1e-4f * power_of_ten(dct),
            .phase_current_c=data[10] * 1e-4f * power_of_ten(dct),

            .phase_active_power_a=data[11] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 0) ? 1.f : -1.f),
            .phase_active_power_b=data[12] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 1) ? 1.f : -1.f),
            .phase_active_power_c=data[13] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 2) ? 1.f : -1.f),
            .total_active_power=data[14] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 3) ? 1.f : -1.f),

            .phase_reactive_power_a=data[15] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 4) ? 1.f : -1.f),
            .phase_reactive_power_b=data[16] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 5) ? 1.f : -1.f),
            .phase_reactive_power_c=data[17] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 6) ? 1.f : -1.f),
            .total_reactive_power=data[18] * 1e-4f * power_of_ten(dpq) * (sign & (1 << 7) ? 1.f : -1.f),

            .phase_power_factor_a=data[19] / 1000.f,
            .phase_power_factor_b=data[20] / 1000.f,
            .phase_power_factor_c=data[21] / 1000.f,
            .total_power_factor=data[22] / 1000.f,

            .phase_apparent_power_a=data[23] * 1e-4f * power_of_ten(dpq),
            .phase_apparent_power_b=data[24] * 1e-4f * power_of_ten(dpq),
            .phase_apparent_power_c=data[25] * 1e-4f * power_of_ten(dpq),
            .total_apparent_power=data[26] * 1e-4f * power_of_ten(dpq),

            .frequency=data[27] * 100.f
        };

        res.print();

        return etl::Ok(res);
    }
};

JSON_DEFINE(Larkin,
    JSON_ITEM("phase_voltage_a", phase_voltage_a),
    JSON_ITEM("phase_voltage_b", phase_voltage_b),
    JSON_ITEM("phase_voltage_c", phase_voltage_c),

    JSON_ITEM("line_voltage_ab", line_voltage_ab),
    JSON_ITEM("line_voltage_bc", line_voltage_bc),
    JSON_ITEM("line_voltage_ca", line_voltage_ca),

    JSON_ITEM("phase_current_a", phase_current_a),
    JSON_ITEM("phase_current_b", phase_current_b),
    JSON_ITEM("phase_current_c", phase_current_c),

    JSON_ITEM("phase_active_power_a", phase_active_power_a),
    JSON_ITEM("phase_active_power_b", phase_active_power_b),
    JSON_ITEM("phase_active_power_c", phase_active_power_c),
    JSON_ITEM("total_active_power", total_active_power),

    JSON_ITEM("phase_reactive_power_a", phase_reactive_power_a),
    JSON_ITEM("phase_reactive_power_b", phase_reactive_power_b),
    JSON_ITEM("phase_reactive_power_c", phase_reactive_power_c),
    JSON_ITEM("total_reactive_power", total_reactive_power),

    JSON_ITEM("phase_power_factor_a", phase_power_factor_a),
    JSON_ITEM("phase_power_factor_b", phase_power_factor_b),
    JSON_ITEM("phase_power_factor_c", phase_power_factor_c),
    JSON_ITEM("total_power_factor", total_power_factor),

    JSON_ITEM("phase_apparent_power_a", phase_apparent_power_a),
    JSON_ITEM("phase_apparent_power_b", phase_apparent_power_b),
    JSON_ITEM("phase_apparent_power_c", phase_apparent_power_c),
    JSON_ITEM("total_apparent_power", total_apparent_power),

    JSON_ITEM("frequency", frequency)
)

void Larkin::print() {
    std::cout << etl::json::serialize(*this) << '\n';
}

void larkin_init(http::Server& app) {
    app.Get("/larkin", std::tuple{
        http::arg::arg("address"),
        http::arg::default_val("port", std::string("auto")), 
        http::arg::default_val("baud", 9600),
    }, 
    [](int address, std::string port, int baud) -> http::Server::Result<Larkin> {
        return Client::New(__FILE__, __LINE__, {address, port, baud})
        .and_then([](Client cli) -> modbus::Result<std::vector<uint16_t>> {
            auto chunk1 =  cli.ReadHoldingRegisters(Larkin::RegisterAddress, Larkin::RegisterSize / 2);
            if (chunk1.is_err()) {
                return etl::Err(chunk1.unwrap_err());
            }

            std::this_thread::sleep_for(32ms);
            auto chunk2 =  cli.ReadHoldingRegisters(Larkin::RegisterAddress + Larkin::RegisterSize / 2, Larkin::RegisterSize / 2);
            if (chunk2.is_err()) {
                return etl::Err(chunk2.unwrap_err());
            }

            std::vector<uint16_t> res (chunk1.unwrap().begin(), chunk1.unwrap().end());
            res.insert(res.end(), chunk2.unwrap().begin(), chunk2.unwrap().end());

            return etl::Ok(std::move(res));
        })
        .except([](modbus::Error err) {
            return http::Server::Error{http::StatusInternalServerError, err.what + ": " + std::to_string(err.code)};
        })
        .and_then(Larkin::New);
    });
}