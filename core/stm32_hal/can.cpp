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


#if defined(DELAMETA_STM32_USE_HAL_CAN1) || defined(DELAMETA_STM32_USE_HAL_CAN2) || \
    defined(DELAMETA_STM32_USE_HAL_CAN3) || defined(DELAMETA_STM32_USE_HAL_CAN)
#define DELAMETA_STM32_HAS_CAN
#endif

#ifdef DELAMETA_STM32_HAS_CAN
struct file_descriptor_can_t {
    CAN_HandleTypeDef* handler;
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

#if defined DELAMETA_STM32_USE_HAL_CAN
extern CAN_HandleTypeDef hcan;
static CAN_HandleTypeDef* can_handler = &hcan;
#elif defined DELAMETA_STM32_USE_HAL_CAN1
extern CAN_HandleTypeDef hcan1;
static CAN_HandleTypeDef* can_handler = &hcan1;
#elif defined DELAMETA_STM32_USE_HAL_CAN2
extern CAN_HandleTypeDef hcan2;
static CAN_HandleTypeDef* can_handler = &hcan2;
#elif defined DELAMETA_STM32_USE_HAL_CAN3
extern CAN_HandleTypeDef hcan3;
static CAN_HandleTypeDef* can_handler = &hcan3;
#endif

file_descriptor_can_t file_descriptor_can_instance {can_handler, "/can", 0, nullptr, 0};

static osThreadId_t can_read_thd;
static osSemaphoreId_t can_read_sem;
static StaticSemaphore_t can_read_sem_cb; 
static uint8_t can_rx_buffer[8];

CAN_FilterTypeDef delameta_stm32_hal_can_filter = {}; 
CAN_TxHeaderTypeDef delameta_stm32_hal_can_tx_header = {};
uint32_t delameta_stm32_hal_can_tx_mailbox = 0;

#ifdef DELAMETA_STM32_HAL_CAN_USE_FIFO0
static const uint32_t _CAN_RX_FIFO = CAN_RX_FIFO0;
static const uint32_t _CAN_IT_RX_FIFO = CAN_IT_RX_FIFO0_MSG_PENDING;
static const uint32_t _CAN_FILTER_FIFO = CAN_FILTER_FIFO0;
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_) {
#endif
#ifdef DELAMETA_STM32_HAL_CAN_USE_FIFO1
static const uint32_t _CAN_RX_FIFO = CAN_RX_FIFO1;
static const uint32_t _CANIT_RX_FIFO = CAN_IT_RX_FIFO1_MSG_PENDING;
static const uint32_t _CANFILTER_FIFO = CAN_FILTER_FIFO1;
extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan_) {
#endif
    if (hcan_->Instance == file_descriptor_can_instance.handler->Instance) {
        CAN_RxHeaderTypeDef msg = {};
        HAL_CAN_GetRxMessage(can_handler, _CAN_RX_FIFO, &msg, can_rx_buffer);
        file_descriptor_can_instance.received_data = can_rx_buffer;
        file_descriptor_can_instance.received_data_len = msg.DLC;
        osThreadFlagsSet(can_read_thd, 0b1);
    }
}

void file_descriptor_can_t::init() {
    osSemaphoreAttr_t attr = {};
    attr.cb_mem = &can_read_sem_cb;
    attr.cb_size = sizeof(can_read_sem_cb);
    can_read_sem = osSemaphoreNew(1, 1, &attr);

    delameta_stm32_hal_can_tx_header.RTR = CAN_RTR_DATA;
    delameta_stm32_hal_can_tx_header.TransmitGlobalTime = DISABLE;
    HAL_CAN_Start(can_handler);
    HAL_CAN_ActivateNotification(can_handler, _CAN_IT_RX_FIFO);
}

auto file_descriptor_can_t::read(uint32_t tout) -> Result<std::vector<uint8_t>> {
    osThreadFlagsSet(can_read_thd, 0b10); // cancel the awaiting thread
    can_read_thd = osThreadGetId();

    // read until available
    auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
    if (flag & osFlagsError) {
        can_read_thd = nullptr;
        osSemaphoreRelease(can_read_sem);
        return Err(Error::TransferTimeout);
    }
    if (flag & 0b10) {
        return Err(Error{static_cast<int>(flag), "canceled"});
    }

    can_read_thd = nullptr;
    osSemaphoreRelease(can_read_sem);

    if (etl::heap::freeSize < received_data_len)
        return Err(Error{-1, "No memory"});

    return Ok(std::vector<uint8_t>(received_data, received_data + received_data_len));
}

auto file_descriptor_can_t::read_until(uint32_t tout, size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    osThreadFlagsSet(can_read_thd, 0b10); // cancel the awaiting thread
    can_read_thd = osThreadGetId();

    auto start = etl::time::now();
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (etl::time::elapsed(start).tick < tout) {
        auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
        if (flag & osFlagsError) {
            can_read_thd = nullptr;
            osSemaphoreRelease(can_read_sem);
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
            can_read_thd = nullptr;
            osSemaphoreRelease(can_read_sem);
            return Ok(std::move(buffer));
        }
    }

    can_read_thd = nullptr;
    osSemaphoreRelease(can_read_sem);
    return Err(Error::TransferTimeout);
}

auto file_descriptor_can_t::write(uint32_t, std::string_view data) -> Result<void> {
    if (data.size() > 8) 
        return Err(Error{-1, "CAN DLC"});
    
    uint8_t id_type = (__oflag >> 24) & 0xff;
    uint8_t id_val = (__oflag >> 0) & 0xffffff;
    delameta_stm32_hal_can_tx_header.DLC = data.size();
    delameta_stm32_hal_can_tx_header.IDE = id_type;
    
    if (id_type == CAN_ID_EXT)
        delameta_stm32_hal_can_tx_header.ExtId = id_val;
    else if (id_type == CAN_ID_STD)
        delameta_stm32_hal_can_tx_header.StdId = id_val;
    else 
        return Err(Error{-1, "CAN ID"});
    
    if (auto res = HAL_CAN_AddTxMessage(handler, &delameta_stm32_hal_can_tx_header, (uint8_t*)data.data(), &delameta_stm32_hal_can_tx_mailbox); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});
    
    return Ok();
}

auto file_descriptor_can_t::wait_until_ready(uint32_t tout) -> Result<void> {
    if (osSemaphoreAcquire(can_read_sem, tout) != osOK) {
        return Err(Error::TransferTimeout);
    }
    return Ok();
}

#endif