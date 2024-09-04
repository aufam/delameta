#include "main.h" // from Core/Src
#include "delameta/debug.h"
#include "delameta/endpoint.h"
#include "delameta/utils.h"
#include "etl/heap.h"

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

#if defined(DELAMETA_STM32_USE_HAL_SPI1) || defined(DELAMETA_STM32_USE_HAL_SPI2) || \
    defined(DELAMETA_STM32_USE_HAL_SPI3) || defined(DELAMETA_STM32_USE_HAL_SPI4) || \
    defined(DELAMETA_STM32_USE_HAL_SPI5) || defined(DELAMETA_STM32_USE_HAL_SPI6)
#define DELAMETA_STM32_HAS_SPI
#endif

#ifdef DELAMETA_STM32_HAS_SPI
struct SPI_Endpoint_Args {
    SPI_HandleTypeDef* handler;
    uint32_t timeout = HAL_MAX_DELAY;
};

class SPI_Endpoint : public SPI_Endpoint_Args, public Descriptor {
public:
    SPI_Endpoint(SPI_Endpoint_Args args) : SPI_Endpoint_Args(args) {}

    Result<std::vector<uint8_t>> read() override;
    Stream read_as_stream(size_t n) override;
    Result<std::vector<uint8_t>> read_until(size_t n) override;

    Result<void> write(std::string_view data) override;
};

auto SPI_Endpoint::read() -> Result<std::vector<uint8_t>> {
    uint8_t byte;
    if (auto res = HAL_SPI_Receive(handler, &byte, 1, timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::vector<uint8_t>({byte}));
}

auto SPI_Endpoint::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    if (auto res = HAL_SPI_Receive(handler, buffer.data(), buffer.size(), timeout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::move(buffer));
}

auto SPI_Endpoint::read_as_stream(size_t n) -> Stream {
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

auto SPI_Endpoint::write(std::string_view data) -> Result<void> {
    if (auto res = HAL_SPI_Transmit(handler, (uint8_t*)data.data(), data.size(), timeout); res != HAL_OK) 
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok();
}


static Result<Endpoint> EndpointFactorySPI(const char*, int, const URL& uri) {
    SPI_Endpoint_Args args = {};

    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    extern SPI_HandleTypeDef hspi1;
    if (uri.host == "/spi1") args.handler = &hspi1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    extern SPI_HandleTypeDef hspi2;
    if (uri.host == "/spi2") args.handler = &hspi2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    extern SPI_HandleTypeDef hspi3;
    if (uri.host == "/spi3") args.handler = &hspi3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    extern SPI_HandleTypeDef hspi4;
    if (uri.host == "/spi4") args.handler = &hspi4;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    extern SPI_HandleTypeDef hspi5;
    if (uri.host == "/spi5") args.handler = &hspi5;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    extern SPI_HandleTypeDef hspi6;
    if (uri.host == "/spi6") args.handler = &hspi6;
    #endif

    if (args.handler == nullptr) 
        return Err(Error{-1, "Invalid host"});

    auto it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        auto res = string_num_into<uint32_t>(it->second);
        if (res.is_err())
            return Err(Error{-1, res.unwrap_err()});
        args.timeout = res.unwrap();
    } 

    return Ok(new SPI_Endpoint(args));
}

extern std::unordered_map<std::string_view, EndpointFactoryFunction> delameta_endpoints_map;

void delameta_spi_init() {
    delameta_endpoints_map["spi"] = &EndpointFactorySPI;
}

#endif