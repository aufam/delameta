#include "main.h" // from Core/Src
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "delameta/debug.h"
#include "delameta/stream.h"
#include "etl/heap.h"
#include "etl/time.h"
#include <cstring>

using namespace Project;
using namespace Project::delameta;
using etl::Err;
using etl::Ok;

#if defined(DELAMETA_STM32_USE_HAL_UART1) || defined(DELAMETA_STM32_USE_HAL_UART2) || \
    defined(DELAMETA_STM32_USE_HAL_UART3) || defined(DELAMETA_STM32_USE_HAL_UART4) || \
    defined(DELAMETA_STM32_USE_HAL_UART5) || defined(DELAMETA_STM32_USE_HAL_UART6) || \
    defined(DELAMETA_STM32_USE_HAL_UART7) || defined(DELAMETA_STM32_USE_HAL_UART8)
#define DELAMETA_STM32_HAS_UART
#endif

#ifdef DELAMETA_STM32_HAS_UART
struct uart_handler_t {
    UART_HandleTypeDef* huart;
    osThreadId_t uart_read_thd;
    osSemaphoreId_t uart_read_sem;
    StaticSemaphore_t uart_read_sem_cb; 
    uint8_t uart_rx_buffer[128];
};

struct file_descriptor_uart_t {
    uart_handler_t* handler;
    std::string_view __file;
    int __oflag;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_UART1
extern UART_HandleTypeDef huart1;
static uart_handler_t uart1_handler {&huart1, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance1 {&uart1_handler, "/uart1", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART2
extern UART_HandleTypeDef huart2;
static uart_handler_t uart2_handler {&huart2, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance2 {&uart2_handler, "/uart2", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART3
extern UART_HandleTypeDef huart3;
static uart_handler_t uart3_handler {&huart3, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance3 {&uart3_handler, "/uart3", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART4
extern UART_HandleTypeDef huart4;
static uart_handler_t uart4_handler {&huart4, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance4 {&uart4_handler, "/uart4", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART5
extern UART_HandleTypeDef huart5;
static uart_handler_t uart5_handler {&huart5, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance5 {&uart5_handler, "/uart5", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART6
extern UART_HandleTypeDef huart6;
static uart_handler_t uart6_handler {&huart6, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance6 {&uart6_handler, "/uart6", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART7
extern UART_HandleTypeDef huart7;
static uart_handler_t uart7_handler {&huart7, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance7 {&uart7_handler, "/uart7", 0, nullptr, 0};
#endif

#ifdef DELAMETA_STM32_USE_HAL_UART8
extern UART_HandleTypeDef huart8;
static uart_handler_t uart8_handler {&huart8, nullptr, nullptr, {}, {}};
file_descriptor_uart_t file_descriptor_uart_instance8 {&uart8_handler, "/uart8", 0, nullptr, 0};
#endif

void file_descriptor_uart_t::init() {
    HAL_UARTEx_ReceiveToIdle_DMA(handler->huart, handler->uart_rx_buffer, sizeof(handler->uart_rx_buffer));
    __HAL_DMA_DISABLE_IT(handler->huart->hdmarx, DMA_IT_HT);

    osSemaphoreAttr_t attr = {};
    attr.cb_mem = &handler->uart_read_sem_cb;
    attr.cb_size = sizeof(handler->uart_read_sem_cb);
    handler->uart_read_sem = osSemaphoreNew(1, 1, &attr);
}

auto file_descriptor_uart_t::read(uint32_t tout) -> Result<std::vector<uint8_t>> {
    osThreadFlagsSet(handler->uart_read_thd, 0b10); // cancel the awaiting thread
    handler->uart_read_thd = osThreadGetId();

    // read until available
    auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
    if (flag & osFlagsError) {
        handler->uart_read_thd = nullptr;
        osSemaphoreRelease(handler->uart_read_sem);
        return Err(Error::TransferTimeout);
    }
    if (flag & 0b10) {
        return Err(Error{static_cast<int>(flag), "canceled"});
    }

    handler->uart_read_thd = nullptr;
    osSemaphoreRelease(handler->uart_read_sem);

    if (etl::heap::freeSize < received_data_len)
        return Err(Error{-1, "No memory"});

    return Ok(std::vector<uint8_t>(received_data, received_data + received_data_len));
}

auto file_descriptor_uart_t::read_until(uint32_t tout, size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    osThreadFlagsSet(handler->uart_read_thd, 0b10); // cancel the awaiting thread
    handler->uart_read_thd = osThreadGetId();

    auto start = etl::time::now();
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (etl::time::elapsed(start).tick < tout) {
        auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
        if (flag & osFlagsError) {
            handler->uart_read_thd = nullptr;
            osSemaphoreRelease(handler->uart_read_sem);
            return Err(Error::TransferTimeout);
        }
        if (flag & 0b10) {
            return Err(Error{static_cast<int>(flag), "canceled"});
        }

        auto size = std::min(remaining_size, received_data_len);
        ::memcpy(ptr, received_data, size);

        ptr += size;
        remaining_size -= size;

        if (remaining_size == 0) {
            handler->uart_read_thd = nullptr;
            osSemaphoreRelease(handler->uart_read_sem);
            return Ok(std::move(buffer));
        }
    }

    handler->uart_read_thd = nullptr;
    osSemaphoreRelease(handler->uart_read_sem);
    return Err(Error::TransferTimeout);
}

auto file_descriptor_uart_t::write(uint32_t tout, std::string_view data) -> Result<void> {
    while (huart1.gState != HAL_UART_STATE_READY);
    if (auto res = HAL_UART_Transmit(handler->huart, (const uint8_t*) data.data(), data.size(), tout); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});
    return Ok();
}

auto file_descriptor_uart_t::wait_until_ready(uint32_t tout) -> Result<void> {
    if (osSemaphoreAcquire(handler->uart_read_sem, tout) != osOK) {
        return Err(Error::TransferTimeout);
    }
    return Ok();
}

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    file_descriptor_uart_t* uart_fd = nullptr;
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    if (huart->Instance == huart1.Instance) {uart_fd = &file_descriptor_uart_instance1;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    if (huart->Instance == huart2.Instance) {uart_fd = &file_descriptor_uart_instance2;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    if (huart->Instance == huart3.Instance) {uart_fd = &file_descriptor_uart_instance3;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    if (huart->Instance == huart4.Instance) {uart_fd = &file_descriptor_uart_instance4;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    if (huart->Instance == huart5.Instance) {uart_fd = &file_descriptor_uart_instance5;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    if (huart->Instance == huart6.Instance) {uart_fd = &file_descriptor_uart_instance6;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    if (huart->Instance == huart7.Instance) {uart_fd = &file_descriptor_uart_instance7;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    if (huart->Instance == huart8.Instance) {uart_fd = &file_descriptor_uart_instance8;}
    #endif
    if (uart_fd) {
        uart_fd->received_data = uart_fd->handler->uart_rx_buffer;
        uart_fd->received_data_len = Size;
        osThreadFlagsSet(uart_fd->handler->uart_read_thd, 0b1);
        HAL_UARTEx_ReceiveToIdle_DMA(
            uart_fd->handler->huart, 
            uart_fd->handler->uart_rx_buffer, 
            sizeof(uart_fd->handler->uart_rx_buffer)
        );
    }
}
#endif