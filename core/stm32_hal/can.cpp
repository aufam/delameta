#include "main.h" // from Core/Src
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "delameta/debug.h"
#include "delameta/endpoint.h"
#include "delameta/utils.h"
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
class CAN_Endpoint : public Descriptor {
public:
    int timeout;

    CAN_Endpoint(int timeout) : timeout(timeout) {}

    Result<std::vector<uint8_t>> read() override;
    Stream read_as_stream(size_t n) override;
    Result<std::vector<uint8_t>> read_until(size_t n) override;

    Result<void> write(std::string_view data) override;
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

static volatile osThreadId_t can_read_thd;

static uint8_t can_rx_buffer[8];
static CAN_TxHeaderTypeDef can_tx_header;
static CAN_RxHeaderTypeDef can_rx_header;

static uint32_t can_mailbox = 0;
static CAN_FilterTypeDef can_filter;

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
    if (hcan_->Instance == can_handler->Instance) {
        HAL_CAN_GetRxMessage(can_handler, _CAN_RX_FIFO, &can_rx_header, can_rx_buffer);
        osThreadFlagsSet(can_read_thd, 0b1);
    }
}

uint32_t delameta_can_get_rx_id() {
    return can_rx_header.IDE == CAN_ID_EXT ? can_rx_header.ExtId : can_rx_header.StdId;
}

void delameta_can_set_tx_id(uint32_t id) {
    can_tx_header.IDE == CAN_ID_EXT ? (can_tx_header.ExtId = id) : (can_tx_header.ExtId = id);
}

void delameta_can_set_filter(uint32_t filter) {
    if (can_tx_header.IDE == CAN_ID_STD) {
        // 11 bits, left padding, high half-word
        can_filter.FilterIdLow      = 0;
        can_filter.FilterIdHigh     = (filter << (16 - 11)) & 0xFFFFu;
    } else {
        // 18 bits, 3 bits offset, low half-word
        can_filter.FilterIdLow      = (filter << (16 - 13)) & 0xFFFFu;  // 13 bits to low half-word
        can_filter.FilterIdHigh     = (filter >> 13) & 0b11111u;        // 5 bits to high half-word
    }
    HAL_CAN_ConfigFilter(can_handler, &can_filter);
}

void delameta_can_set_mask(uint32_t mask) {
    if (can_tx_header.IDE == CAN_ID_STD) {
        // 11 bits, left padding, high half-word
        can_filter.FilterMaskIdLow      = 0;
        can_filter.FilterMaskIdHigh     = (mask << (16 - 11)) & 0xFFFFu;
    } else {
        // 18 bits, 3 bits offset, low half-word
        can_filter.FilterMaskIdLow      = (mask << 3) & 0xFFFFu;          // 13 bits to low half-word
        can_filter.FilterMaskIdHigh     = (mask >> 13) & 0b11111u;        // 5 bits to high half-word
    }
    HAL_CAN_ConfigFilter(can_handler, &can_filter);
}

auto CAN_Endpoint::read() -> Result<std::vector<uint8_t>> {
    osThreadFlagsSet(can_read_thd, 0b10); // cancel the awaiting thread
    can_read_thd = osThreadGetId();

    // read until available
    uint32_t tout = timeout < 0 ? osWaitForever : (uint32_t)timeout * 1000;
    auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
    if (flag & osFlagsError) {
        can_read_thd = nullptr;
        return Err(Error::TransferTimeout);
    }
    if (flag & 0b10) {
        return Err(Error{static_cast<int>(flag), "canceled"});
    }

    can_read_thd = nullptr;

    if (etl::heap::freeSize < can_rx_header.DLC)
        return Err(Error{-1, "No memory"});

    return Ok(std::vector<uint8_t>(can_rx_buffer, can_rx_buffer + can_rx_header.DLC));
}

auto CAN_Endpoint::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    osThreadFlagsSet(can_read_thd, 0b10); // cancel the awaiting thread
    can_read_thd = osThreadGetId();

    auto start = etl::time::now();
    size_t remaining_size = n;
    auto ptr = buffer.data();

    uint32_t tout = timeout < 0 ? osWaitForever : (uint32_t)timeout * 1000;

    while (etl::time::elapsed(start).tick < tout) {
        auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
        if (flag & osFlagsError) {
            can_read_thd = nullptr;
            return Err(Error::TransferTimeout);
        }
        if (flag & 0b10) {
            return Err(Error{static_cast<int>(flag), "canceled"});
        }

        auto size = std::min((uint32_t)remaining_size, can_rx_header.DLC);
        ::memcpy(ptr, can_rx_buffer, size);

        ptr += size;
        remaining_size -= size;

        if (remaining_size == 0) {
            can_read_thd = nullptr;
            return Ok(std::move(buffer));
        }
    }

    can_read_thd = nullptr;
    return Err(Error::TransferTimeout);
}

auto CAN_Endpoint::read_as_stream(size_t n) -> Stream {
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

auto CAN_Endpoint::write(std::string_view data) -> Result<void> {
    if (data.size() > 8) 
        return Err(Error{-1, "CAN DLC"});
    
    can_tx_header.DLC = data.size();
    
    if (auto res = HAL_CAN_AddTxMessage(can_handler, &can_tx_header, (uint8_t*)data.data(), &can_mailbox); res != HAL_OK)
        return Err(Error{static_cast<int>(res), "hal error"});
    
    return Ok();
}

static Result<Endpoint> EndpointFactoryCAN(const char*, int, const URL& uri) {
    uint32_t timeout = -1;

    auto it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        auto res = string_num_into<uint32_t>(it->second);
        if (res.is_err())
            return Err(Error{-1, res.unwrap_err()});
        timeout = res.unwrap();
    } 

    return Ok(new CAN_Endpoint(timeout));
}

extern std::unordered_map<std::string_view, EndpointFactoryFunction> delameta_endpoints_map;

void delameta_can_init() {
    delameta_endpoints_map["can"] = &EndpointFactoryCAN;

    can_filter.FilterActivation = CAN_FILTER_ENABLE;
    can_filter.FilterFIFOAssignment = _CAN_FILTER_FIFO;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterBank = 10;
    can_filter.SlaveStartFilterBank = 0;
    HAL_CAN_ConfigFilter(can_handler, &can_filter);

    can_tx_header.RTR = CAN_RTR_DATA;
    can_tx_header.TransmitGlobalTime = DISABLE;
    can_tx_header.IDE = CAN_ID_EXT;
    HAL_CAN_Start(can_handler);
    HAL_CAN_ActivateNotification(can_handler, _CAN_IT_RX_FIFO);
}

#endif