#include "main.h" // from Core/Src
#include "delameta/debug.h"
#include "delameta/endpoint.h"
#include "delameta/utils.h"
#include "etl/heap.h"

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;


#if defined(DELAMETA_STM32_USE_HAL_I2C1) || defined(DELAMETA_STM32_USE_HAL_I2C2) || \
    defined(DELAMETA_STM32_USE_HAL_I2C3) || defined(DELAMETA_STM32_USE_HAL_I2C4) || \
    defined(DELAMETA_STM32_USE_HAL_I2C5)
#define DELAMETA_STM32_HAS_I2C
#endif

#ifdef DELAMETA_STM32_HAS_I2C
struct I2C_Endpoint_Args {
    I2C_HandleTypeDef* handler;
    uint16_t device_address;
    uint16_t mem_address;
    uint32_t timeout = HAL_MAX_DELAY;
};

class I2C_Endpoint : public I2C_Endpoint_Args, public Descriptor {
public:
    I2C_Endpoint(I2C_Endpoint_Args args) : I2C_Endpoint_Args(args) {}

    Result<std::vector<uint8_t>> read() override;
    Stream read_as_stream(size_t n) override;
    Result<std::vector<uint8_t>> read_until(size_t n) override;

    Result<void> write(std::string_view data) override;
};

auto I2C_Endpoint::read() -> Result<std::vector<uint8_t>> {
    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});
    
    uint8_t byte;
    if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, &byte, 1, timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::vector<uint8_t>({byte}));
}

auto I2C_Endpoint::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, buffer.data(), buffer.size(), timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::move(buffer));
}

auto I2C_Endpoint::read_as_stream(size_t n) -> Stream {
    Stream s;

    s << [this, total=n, buffer=std::vector<uint8_t>{}](Stream& s) mutable -> std::string_view {
        buffer = {};
        size_t n = std::min(total, (size_t)128);
        auto data = this->read_until(n);

        if (data.is_ok()) {
            buffer = std::move(data.unwrap());
            total -= n;
            s.again = total > 0;
        }

        return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    };

    return s;
}

auto I2C_Endpoint::write(std::string_view data) -> Result<void> {
    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    if (auto res = HAL_I2C_Mem_Write(handler, device_address, mem_address, 1, (uint8_t*)data.data(), data.size(), timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok();
}

static Result<Endpoint> EndpointFactoryI2C(const char*, int, const URL& uri) {
    I2C_Endpoint_Args args = {};

    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    extern I2C_HandleTypeDef hi2c1;
    if (uri.host == "/i2c1") args.handler = &hi2c1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    extern I2C_HandleTypeDef hi2c2;
    if (uri.host == "/i2c2") args.handler = &hi2c2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    extern I2C_HandleTypeDef hi2c3;
    if (uri.host == "/i2c3") args.handler = &hi2c3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    extern I2C_HandleTypeDef hi2c4;
    if (uri.host == "/i2c4") args.handler = &hi2c4;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    extern I2C_HandleTypeDef hi2c5;
    if (uri.host == "/i2c5") args.handler = &hi2c5;
    #endif

    if (args.handler == nullptr) 
        return Err(Error{-1, "Invalid host"});

    auto it = uri.queries.find("device-address");
    if (it != uri.queries.end()) {
        auto res = string_num_into<uint32_t>(it->second);
        if (res.is_err())
            return Err(Error{-1, res.unwrap_err()});
        args.timeout = res.unwrap();
    } else {
        return Err(Error{-1, "missing device-address"});
    }

    it = uri.queries.find("mem-address");
    if (it != uri.queries.end()) {
        auto res = string_num_into<uint32_t>(it->second);
        if (res.is_err())
            return Err(Error{-1, res.unwrap_err()});
        args.timeout = res.unwrap();
    } else {
        return Err(Error{-1, "missing mem-address"});
    }

    it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        auto res = string_num_into<uint32_t>(it->second);
        if (res.is_err())
            return Err(Error{-1, res.unwrap_err()});
        args.timeout = res.unwrap();
    } 

    return Ok(new I2C_Endpoint(args));
}

extern std::unordered_map<std::string_view, EndpointFactoryFunction> delameta_endpoints_map;

void delameta_i2c_init() {
    delameta_endpoints_map["i2c"] = &EndpointFactoryI2C;
}

#endif
