#include <boost/preprocessor.hpp>
#include "delameta/http/server.h"
#include "delameta/modbus/client.h"
#include "delameta/serial/client.h"
#include "delameta/debug.h"
#include <chrono>
#include <thread>

using namespace Project;
using namespace delameta;
using namespace std::literals;
using delameta::modbus::Client;
using Serial = delameta::serial::Client;

static float power_of_ten(int exponent) {
    float result = 1.0;
    for (int i = 0; i < exponent; ++i) {
        result *= 10;
    }
    return result;
}

static constexpr uint16_t RegisterAddress = 0x0023; 
static constexpr uint16_t RegisterSize = 28;

JSON_DECLARE(
    (Larkin),

    (float, phase_voltage_a)
    (float, phase_voltage_b)
    (float, phase_voltage_c)
    
    (float, line_voltage_ab)
    (float, line_voltage_bc)
    (float, line_voltage_ca)
    
    (float, phase_current_a)
    (float, phase_current_b)
    (float, phase_current_c)
    
    (float, phase_active_power_a)
    (float, phase_active_power_b)
    (float, phase_active_power_c)
    (float, total_active_power)

    (float, phase_reactive_power_a)
    (float, phase_reactive_power_b)
    (float, phase_reactive_power_c)
    (float, total_reactive_power)

    (float, phase_power_factor_a)
    (float, phase_power_factor_b)
    (float, phase_power_factor_c)
    (float, total_power_factor)

    (float, phase_apparent_power_a)
    (float, phase_apparent_power_b)
    (float, phase_apparent_power_c)
    (float, total_apparent_power)

    (float, frequency)
)

static http::Server::Result<Larkin> LarkinNew(const std::vector<uint16_t>& data) {
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

    return etl::Ok(res);
}

HTTP_EXTERN_OBJECT(app);

static HTTP_ROUTE(
    ("/larkin", ("GET")),
    (get_larkin),
        (int        , address, http::arg::arg("address")                            )
        (std::string, port   , http::arg::default_val("port", std::string("auto"))  )
        (int        , baud   , http::arg::default_val("baud", 9600)                 )
        (int        , tout   , http::arg::default_val("tout", 5)                    ),
    (http::Server::Result<Larkin>)
) {
    return Serial::New(FL, {port, baud, tout})
    .and_then([address](Serial session) -> modbus::Result<std::vector<uint16_t>> {
        Client cli(address, session);
        auto chunk1 =  cli.ReadHoldingRegisters(RegisterAddress, RegisterSize / 2);
        if (chunk1.is_err()) {
            return etl::Err(chunk1.unwrap_err());
        }

        std::this_thread::sleep_for(32ms);
        auto chunk2 =  cli.ReadHoldingRegisters(RegisterAddress + RegisterSize / 2, RegisterSize / 2);
        if (chunk2.is_err()) {
            return etl::Err(chunk2.unwrap_err());
        }

        std::vector<uint16_t> res (chunk1.unwrap().begin(), chunk1.unwrap().end());
        res.insert(res.end(), chunk2.unwrap().begin(), chunk2.unwrap().end());

        return etl::Ok(std::move(res));
    })
    .and_then(LarkinNew);
}
