#include "main.h" // from Core/Src

#ifdef DELAMETA_STM32_USE_HAL_USB
#include "usbd_cdc_if.h"
#endif

#include "delameta/file_descriptor.h"
#include "delameta/debug.h"
#include "etl/time.h"
#include "etl/heap.h"

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

using etl::Err;
using etl::Ok;

struct file_descriptor_t {
    const char* __file;
    int __oflag;
    osThreadId_t owner;
    const uint8_t* received_data;
    size_t received_data_len;
};

#define MAX_HANDLE_SZ 128

#ifdef DELAMETA_STM32_USE_HAL_UART1
extern UART_HandleTypeDef huart1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART2
extern UART_HandleTypeDef huart2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART3
extern UART_HandleTypeDef huart3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART4
extern UART_HandleTypeDef huart4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART5
extern UART_HandleTypeDef huart5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART6
extern UART_HandleTypeDef huart6;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART7
extern UART_HandleTypeDef huart7;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART8
extern UART_HandleTypeDef huart8;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C1
extern I2C_HandleTypeDef hi2c1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C2
extern I2C_HandleTypeDef hi2c2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C3
extern I2C_HandleTypeDef hi2c3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C4
extern I2C_HandleTypeDef hi2c4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C5
extern I2C_HandleTypeDef hi2c5;
#endif
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
#ifdef DELAMETA_STM32_USE_HAL_SPI1
extern SPI_HandleTypeDef hspi1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI2
extern SPI_HandleTypeDef hspi2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI3
extern SPI_HandleTypeDef hspi3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI4
extern SPI_HandleTypeDef hspi4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI5
extern SPI_HandleTypeDef hspi5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI6
extern SPI_HandleTypeDef hspi6;
#endif

#if defined(DELAMETA_STM32_USE_HAL_UART1) || defined(DELAMETA_STM32_USE_HAL_UART2) || \
    defined(DELAMETA_STM32_USE_HAL_UART3) || defined(DELAMETA_STM32_USE_HAL_UART4) || \
    defined(DELAMETA_STM32_USE_HAL_UART5) || defined(DELAMETA_STM32_USE_HAL_UART6) || \
    defined(DELAMETA_STM32_USE_HAL_UART7) || defined(DELAMETA_STM32_USE_HAL_UART8)
#define DELAMETA_STM32_HAS_UART
#endif

#if defined(DELAMETA_STM32_USE_HAL_I2C1) || defined(DELAMETA_STM32_USE_HAL_I2C2) || \
    defined(DELAMETA_STM32_USE_HAL_I2C3) || defined(DELAMETA_STM32_USE_HAL_I2C4) || \
    defined(DELAMETA_STM32_USE_HAL_I2C5)
#define DELAMETA_STM32_HAS_I2C
#endif

#if defined(DELAMETA_STM32_USE_HAL_CAN1) || defined(DELAMETA_STM32_USE_HAL_CAN2) || \
    defined(DELAMETA_STM32_USE_HAL_CAN3) || defined(DELAMETA_STM32_USE_HAL_CAN)
#define DELAMETA_STM32_HAS_CAN
#endif

#if defined(DELAMETA_STM32_USE_HAL_SPI1) || defined(DELAMETA_STM32_USE_HAL_SPI2) || \
    defined(DELAMETA_STM32_USE_HAL_SPI3) || defined(DELAMETA_STM32_USE_HAL_SPI4) || \
    defined(DELAMETA_STM32_USE_HAL_SPI5) || defined(DELAMETA_STM32_USE_HAL_SPI6)
#define DELAMETA_STM32_HAS_SPI
#endif

enum file_descriptor_n {
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    I_UART1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    I_UART2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    I_UART3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    I_UART4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    I_UART5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    I_UART6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    I_UART7,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    I_UART8,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    I_I2C1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    I_I2C2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    I_I2C3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    I_I2C4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    I_I2C5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN
    I_CAN,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN1
    I_CAN1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN2
    I_CAN2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN3
    I_CAN3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    I_SPI1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    I_SPI2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    I_SPI3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    I_SPI4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    I_SPI5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    I_SPI6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    I_USB,
    #endif
};

static file_descriptor_t file_descriptors[] = {
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    {"/uart1", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    {"/uart2", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    {"/uart3", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    {"/uart4", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    {"/uart5", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    {"/uart6", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    {"/uart7", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    {"/uart8", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    {"/i2c1", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    {"/i2c2", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    {"/i2c3", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    {"/i2c4", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    {"/i2c5", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN
    {"/can", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN1
    {"/can1", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN2
    {"/can2", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN3
    {"/can3", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    {"/spi1", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    {"/spi2", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    {"/spi3", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    {"/spi4", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    {"/spi5", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    {"/spi6", 0, 0, 0, 0},
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    {"/usb", 0, 0, 0, 0},
    #endif
};

#ifdef DELAMETA_STM32_HAS_UART
static UART_HandleTypeDef* get_uart(int fd) {
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    if (fd == I_UART1) return &huart1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    if (fd == I_UART2) return &huart2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    if (fd == I_UART3) return &huart3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    if (fd == I_UART4) return &huart4;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    if (fd == I_UART5) return &huart5;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    if (fd == I_UART6) return &huart6;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    if (fd == I_UART7) return &huart7;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    if (fd == I_UART8) return &huart8;
    #endif
    return nullptr;
}

    #ifdef DELAMETA_STM32_USE_HAL_UART1
    static uint8_t uart1_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    static uint8_t uart2_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    static uint8_t uart3_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    static uint8_t uart4_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    static uint8_t uart5_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    static uint8_t uart6_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    static uint8_t uart7_rx_buffer[MAX_HANDLE_SZ];
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    static uint8_t uart8_rx_buffer[MAX_HANDLE_SZ];
    #endif
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    UART_HandleTypeDef* handler = nullptr;
    file_descriptor_t* desc = nullptr;
    uint8_t* buf = nullptr;
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    if (huart->Instance == huart1.Instance) {handler = huart; desc = &file_descriptors[I_UART1]; buf = uart1_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    if (huart->Instance == huart2.Instance) {handler = huart; desc = &file_descriptors[I_UART2]; buf = uart2_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    if (huart->Instance == huart3.Instance) {handler = huart; desc = &file_descriptors[I_UART3]; buf = uart3_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    if (huart->Instance == huart4.Instance) {handler = huart; desc = &file_descriptors[I_UART4]; buf = uart4_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    if (huart->Instance == huart5.Instance) {handler = huart; desc = &file_descriptors[I_UART5]; buf = uart5_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    if (huart->Instance == huart6.Instance) {handler = huart; desc = &file_descriptors[I_UART6]; buf = uart6_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    if (huart->Instance == huart7.Instance) {handler = huart; desc = &file_descriptors[I_UART7]; buf = uart7_rx_buffer;}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    if (huart->Instance == huart8.Instance) {handler = huart; desc = &file_descriptors[I_UART8]; buf = uart8_rx_buffer;}
    #endif
    if (handler) {
        if (desc->owner) {
            desc->received_data = buf;
            desc->received_data_len = Size;
            osThreadFlagsSet(desc->owner, 0b1);
        }
        HAL_UARTEx_ReceiveToIdle_DMA(handler, buf, MAX_HANDLE_SZ);
    }
}
#endif
#ifdef DELAMETA_STM32_HAS_I2C
static I2C_HandleTypeDef* get_i2c(int fd) {
    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    if (fd == I_I2C1) return &hi2c1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    if (fd == I_I2C2) return &hi2c2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    if (fd == I_I2C3) return &hi2c3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    if (fd == I_I2C4) return &hi2c4;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    if (fd == I_I2C5) return &hi2c5;
    #endif
    return nullptr;
}
#endif
#ifdef DELAMETA_STM32_HAS_SPI
static SPI_HandleTypeDef* get_spi(int fd) {
    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    if (fd == I_SPI1) return &hspi1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    if (fd == I_SPI2) return &hspi2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    if (fd == I_SPI3) return &hspi3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    if (fd == I_SPI4) return &hspi4;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    if (fd == I_SPI5) return &hspi5;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    if (fd == I_SPI6) return &hspi6;
    #endif
    return nullptr;
}
#endif
#ifdef DELAMETA_STM32_HAS_CAN
CAN_FilterTypeDef delameta_stm32_hal_can_filter = {}; 
CAN_TxHeaderTypeDef delameta_stm32_hal_can_tx_header = {};
uint32_t delameta_stm32_hal_can_tx_mailbox = 0;
static uint8_t can_rx_buffer[8];
static CAN_HandleTypeDef* get_can(int fd) {
    #ifdef DELAMETA_STM32_USE_HAL_CAN
    if (fd == I_CAN) return &hcan;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN1
    if (fd == I_CAN1) return &hcan1;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN2
    if (fd == I_CAN2) return &hcan2;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN3
    if (fd == I_CAN3) return &hcan3;
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN4
    if (fd == I_CAN4) return &hcan4;
    #endif
    return nullptr;
}
 
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
    CAN_HandleTypeDef* handler = nullptr;
    file_descriptor_t* desc = nullptr;
    if (hcan_->Instance == can_handler->Instance)
    #ifdef DELAMETA_STM32_USE_HAL_CAN
    {handler = hcan_; desc = &file_descriptors[I_CAN];}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN1
    {handler = hcan_; desc = &file_descriptors[I_CAN1];}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN2
    {handler = hcan_; desc = &file_descriptors[I_CAN2];}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN3
    {handler = hcan_; desc = &file_descriptors[I_CAN3];}
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_CAN4
    {handler = hcan_; desc = &file_descriptors[I_CAN4];}
    #endif
    if (handler) {
        CAN_RxHeaderTypeDef msg = {};
        HAL_CAN_GetRxMessage(can_handler, _CAN_RX_FIFO, &msg, can_rx_buffer);
        if (desc->owner) {
            desc->received_data = can_rx_buffer;
            desc->received_data_len = msg.DLC;
            osThreadFlagsSet(desc->owner, 0b1);
        }
    }
}
#endif
#ifdef DELAMETA_STM32_USE_HAL_USB
// static osThreadId_t usb_write_thd;
static StaticSemaphore_t usb_write_sem_cb; 
static osSemaphoreId_t usb_write_sem;
extern "C" void CDC_ReceiveCplt_Callback(const uint8_t *pbuf, uint32_t len) {
    auto& desc = file_descriptors[I_USB];
    if (desc.owner) {
        desc.received_data = pbuf;
        desc.received_data_len = len;
        osThreadFlagsSet(desc.owner, 0b1);
    }
}
extern "C" void CDC_TransmitCplt_Callback(const uint8_t *pbuf, uint32_t len) {
    UNUSED(pbuf);
    UNUSED(len);
    osSemaphoreRelease(usb_write_sem);
    // osThreadFlagsSet(usb_write_thd, 0b10);
}
#endif

FileDescriptor::FileDescriptor(const char* file, int line, int fd)
    : fd(fd)
    , timeout(-1)
    , file(file)
    , line(line) {}

FileDescriptor::FileDescriptor(FileDescriptor&& other) 
    : fd(std::exchange(other.fd, -1))
    , timeout(other.timeout)
    , file(other.file)
    , line(other.line) {}

FileDescriptor::~FileDescriptor() {
    if (fd >= 0) {
        file_descriptors[fd].owner = nullptr;
        fd = -1;
    }
}

auto FileDescriptor::Open(const char* file, int line, const char* __file, int __oflag) -> Result<FileDescriptor> {
    for (size_t i = 0; i < std::size(file_descriptors); ++i) {
        auto &fd = file_descriptors[i];
        if (std::string_view(fd.__file) == __file) {
            while (fd.owner) { etl::time::sleep(10ms); }; // wait until available
            fd.owner = osThreadGetId();
            fd.__oflag = __oflag;
            return Ok(FileDescriptor(file, line, i));
        }
    }

    return Err(Error{-1, "no __file"});
}

auto FileDescriptor::read() -> Result<std::vector<uint8_t>> {
    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    auto& desc = file_descriptors[fd];

    // read only one byte for i2c and spi
    #ifdef DELAMETA_STM32_HAS_I2C
    if (auto handler = get_i2c(fd); handler != nullptr) {
        uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
        uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;
        if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});
        
        uint8_t byte;
        if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, &byte, 1, tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});

        return Ok(std::vector<uint8_t>({byte}));
    } 
    #endif
    #ifdef DELAMETA_STM32_HAS_SPI
    if (auto handler = get_spi(fd); handler != nullptr) {
        uint8_t byte;
        if (auto res = HAL_SPI_Receive(handler, &byte, 1, tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});

        return Ok(std::vector<uint8_t>({byte}));
    }
    #endif

    // read until available for other peripherals
    auto flag = osThreadFlagsWait(0b1, osFlagsWaitAny, tout);
    if (flag & osFlagsError)
        return Err(Error{static_cast<int>(flag), "os error"});
    
    if (etl::heap::freeSize < desc.received_data_len) {
        return Err(Error{-1, "No memory"});
    }

    return Ok(std::vector<uint8_t>(desc.received_data, desc.received_data + desc.received_data_len));
}

auto FileDescriptor::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    auto& desc = file_descriptors[fd];

    #ifdef DELAMETA_STM32_HAS_I2C
    if (auto handler = get_i2c(fd); handler != nullptr) {
        uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
        uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;
        if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});

        if (auto res = HAL_I2C_Mem_Read(handler, device_address, mem_address, 1, buffer.data(), buffer.size(), tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});

        return Ok(std::move(buffer));
    } 
    #endif
    #ifdef DELAMETA_STM32_HAS_SPI
    if (auto handler = get_spi(fd); handler != nullptr) {
        if (auto res = HAL_SPI_Receive(handler, buffer.data(), buffer.size(), tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});

        return Ok(std::move(buffer));
    }
    #endif

    auto start = etl::time::now();
    size_t remaining_size = n;
    auto ptr = buffer.data();
    while (etl::time::elapsed(start).tick < tout) {
        auto flag = osThreadFlagsWait(0b1, osFlagsWaitAny, tout);
        if (flag & osFlagsError) {
            return Err(Error{static_cast<int>(flag), "os error"});
        }

        auto size = std::min(remaining_size, desc.received_data_len);
        ::memcpy(ptr, desc.received_data, size);

        ptr += size;
        remaining_size -= size;

        if (remaining_size == 0) {
            return Ok(std::move(buffer));
        }
    }

    return Err(Error::TransferTimeout);
}

auto FileDescriptor::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, MAX_HANDLE_SZ);
        s << [this, size, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = this->read_until(size);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            } else {
                warning(file, line, data.unwrap_err().what);
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total -= size;
    }

    return s;
}

auto FileDescriptor::write(std::string_view data) -> Result<void> {
    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    auto& desc = file_descriptors[fd];

    #ifdef DELAMETA_STM32_HAS_UART
    if (auto handler = get_uart(fd); handler != nullptr) {
        while (huart1.gState != HAL_UART_STATE_READY);
        if (auto res = HAL_UART_Transmit(handler, (const uint8_t*) data.data(), data.size(), tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});
        return Ok();
    }
    #endif
    #ifdef DELAMETA_STM32_HAS_I2C
    if (auto handler = get_i2c(fd); handler != nullptr) {
        uint16_t device_address = (desc.__oflag >> 16) & 0xffff;
        uint16_t mem_address = (desc.__oflag >> 0) & 0xffff;
        if (auto res = HAL_I2C_IsDeviceReady(handler, device_address, 1, tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "186 : hal error"});
        if (auto res = HAL_I2C_Mem_Write(handler, device_address, mem_address, 1, (uint8_t*)data.data(), data.size(), tout); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "188 : hal error"});
        return Ok();
    }
    #endif
    #ifdef DELAMETA_STM32_HAS_CAN
    if (auto handler = get_can(fd); handler != nullptr) {
        if (data.size() > 8) return Err(Error{-1, "CAN DLC"});
        uint8_t id_type = (desc.__oflag >> 24) & 0xff;
        uint8_t id_val = (desc.__oflag >> 0) & 0xffffff;
        delameta_stm32_hal_can_tx_header.DLC = data.size();
        delameta_stm32_hal_can_tx_header.IDE = id_type;
        if (id_type == CAN_ID_EXT)
            delameta_stm32_hal_can_tx_header.ExtId = id_val;
        else if (id_type == CAN_ID_STD)
            delameta_stm32_hal_can_tx_header.StdId = id_val;
        else return Err(Error{-1, "CAN ID"});
        if (auto res = HAL_CAN_AddTxMessage(handler, &delameta_stm32_hal_can_tx_header, (uint8_t*)data.data(), &delameta_stm32_hal_can_tx_mailbox); res != HAL_OK)
            return Err(Error{static_cast<int>(res), "hal error"});
        return Ok();
    }
    #endif
    #ifdef DELAMETA_STM32_HAS_SPI
    if (auto handler = get_spi(fd); handler != nullptr) {
        if (auto res = HAL_SPI_Transmit(handler, (uint8_t*)data.data(), data.size(), tout); res != HAL_OK) 
            return Err(Error{static_cast<int>(res), "hal error"});
        return Ok();
    }
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    if (fd == I_USB) {
        static size_t last_data_size;
        osSemaphoreAcquire(usb_write_sem, last_data_size); // assuming the speed is 1 byte / ms
        if (auto res = CDC_Transmit_FS((uint8_t*)data.data(), data.size()); res != USBD_OK) {
            return Err(Error{res, "USBD busy"});
        } 
        last_data_size = data.size();
        return Ok();
    }
    #endif
    return Err(Error{-1, "fd closed"});
}

auto FileDescriptor::file_size() -> Result<size_t> {
    return Err(Error{-1, "No impl"}); // not implemented yet
}

auto FileDescriptor::operator<<(Stream& other) -> FileDescriptor& {
    other >> *this;
    return *this;
}

auto FileDescriptor::operator>>(Stream& s) -> FileDescriptor& {
    auto [size, size_err] = file_size();
    if (size_err) {
        return *this;
    }

    auto p_fd = new FileDescriptor(std::move(*this));

    for (int total_size = int(*size); total_size > 0;) {
        size_t n = std::min(total_size, MAX_HANDLE_SZ);
        s << [p_fd, n, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
            auto data = p_fd->read_until(n);
            if (data.is_ok()) {
                buffer = std::move(data.unwrap());
            }
            return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
        };
        total_size -= n;
    }

    s << [p_fd]() mutable -> std::string_view {
        delete p_fd;
        return "";
    };

    return *this;
}

// peripheral init init
extern "C" void delameta_stm32_hal_init() {
    #ifdef DELAMETA_STM32_HAS_CAN
    delameta_stm32_hal_can_tx_header.RTR = CAN_RTR_DATA;
    delameta_stm32_hal_can_tx_header.TransmitGlobalTime = DISABLE;
    HAL_CAN_Start(can_handler);
    HAL_CAN_ActivateNotification(can_handler, _CAN_IT_RX_FIFO);
    #endif

    osSemaphoreAttr_t attr = {};
    attr.cb_mem = &usb_write_sem_cb;
    attr.cb_size = sizeof(usb_write_sem_cb);
    usb_write_sem = osSemaphoreNew(1, 1, &attr);

    #ifdef DELAMETA_STM32_USE_HAL_UART1
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_buffer, sizeof(uart1_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_buffer, sizeof(uart2_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart3_rx_buffer, sizeof(uart3_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, uart4_rx_buffer, sizeof(uart4_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    HAL_UARTEx_ReceiveToIdle_DMA(&huart5, uart5_rx_buffer, sizeof(uart5_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart5.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, uart6_rx_buffer, sizeof(uart6_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart6.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    HAL_UARTEx_ReceiveToIdle_DMA(&huart7, uart7_rx_buffer, sizeof(uart7_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    HAL_UARTEx_ReceiveToIdle_DMA(&huart8, uart8_rx_buffer, sizeof(uart8_rx_buffer));
    __HAL_DMA_DISABLE_IT(huart8.hdmarx, DMA_IT_HT);
    #endif
}