#include "main.h" // from Core/Src
#include "delameta/debug.h"
#include "delameta/stream.h"
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
struct file_descriptor_i2c_t {
    I2C_HandleTypeDef* handler;
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

#ifdef DELAMETA_STM32_USE_HAL_I2C1
extern I2C_HandleTypeDef hi2c1;
file_descriptor_i2c_t file_descriptor_i2c_instance1 = {&hi2c1, "/i2c1", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C2
extern I2C_HandleTypeDef hi2c2;
file_descriptor_i2c_t file_descriptor_i2c_instance2 = {&hi2c2, "/i2c2", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C3
extern I2C_HandleTypeDef hi2c3;
file_descriptor_i2c_t file_descriptor_i2c_instance3 = {&hi2c3, "/i2c3", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C4
extern I2C_HandleTypeDef hi2c4;
file_descriptor_i2c_t file_descriptor_i2c_instance4 = {&hi2c4, "/i2c4", 0, nullptr, 0};
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C5
extern I2C_HandleTypeDef hi2c5;
file_descriptor_i2c_t file_descriptor_i2c_instance5 = {&hi2c5, "/i2c5", 0, nullptr, 0};
#endif

void file_descriptor_i2c_t::init() {}

auto file_descriptor_i2c_t::read(uint32_t tout) -> Result<std::vector<uint8_t>> {
    uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
    uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;
    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});
    
    uint8_t byte;
    if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, &byte, 1, tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::vector<uint8_t>({byte}));
}

auto file_descriptor_i2c_t::read_until(uint32_t tout, size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
    uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;
    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, buffer.data(), buffer.size(), tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok(std::move(buffer));
}

auto file_descriptor_i2c_t::write(uint32_t tout, std::string_view data) -> Result<void> {
    uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
    uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;

    if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    if (auto res = HAL_I2C_Mem_Write(handler, device_address, mem_address, 1, (uint8_t*)data.data(), data.size(), tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});

    return Ok();
}

auto file_descriptor_i2c_t::wait_until_ready(uint32_t) -> Result<void> {
    return Ok();
}
#endif