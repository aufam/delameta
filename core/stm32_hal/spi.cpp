#include "main.h" // from Core/Src
#include "delameta/debug.h"
#include "delameta/stream.h"
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
struct file_descriptor_spi_t {
    SPI_HandleTypeDef* handler;
    const char* __file;
    int __oflag;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_SPI1
extern SPI_HandleTypeDef hspi1;
file_descriptor_spi_t file_descriptor_spi_instance1 {&hspi1, "/spi1", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI2
extern SPI_HandleTypeDef hspi2;
file_descriptor_spi_t file_descriptor_spi_instance2 {&hspi2, "/spi2", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI3
extern SPI_HandleTypeDef hspi3;
file_descriptor_spi_t file_descriptor_spi_instance3 {&hspi3, "/spi3", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI4
extern SPI_HandleTypeDef hspi4;
file_descriptor_spi_t file_descriptor_spi_instance4 {&hspi4, "/spi4", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI5
extern SPI_HandleTypeDef hspi5;
file_descriptor_spi_t file_descriptor_spi_instance5 {&hspi5, "/spi5", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI6
extern SPI_HandleTypeDef hspi6;
file_descriptor_spi_t file_descriptor_spi_instance6 {&hspi6, "/spi6", 0, nullptr, 0};
#endif

void file_descriptor_spi_t::init() {}

auto file_descriptor_spi_t::read(uint32_t tout) -> Result<std::vector<uint8_t>> {
    uint8_t byte;
    if (auto res = HAL_SPI_Receive(handler, &byte, 1, tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::vector<uint8_t>({byte}));
}

auto file_descriptor_spi_t::read_until(uint32_t tout, size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    if (auto res = HAL_SPI_Receive(handler, buffer.data(), buffer.size(), tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::move(buffer));
}

auto file_descriptor_spi_t::write(uint32_t tout, std::string_view data) -> Result<void> {
    if (auto res = HAL_SPI_Transmit(handler, (uint8_t*)data.data(), data.size(), tout); res != HAL_OK) 
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok();
}

auto file_descriptor_spi_t::wait_until_ready(uint32_t) -> Result<void> {
    return Ok();
}

#endif