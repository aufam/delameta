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

#ifdef DELAMETA_STM32_USE_HAL_USB
#include "usbd_cdc_if.h"

struct usb_handler_t {
    osThreadId_t usb_read_thd;
    osSemaphoreId_t usb_read_sem;
    osSemaphoreId_t usb_write_sem;
    StaticSemaphore_t usb_read_sem_cb; 
    StaticSemaphore_t usb_write_sem_cb; 
};

struct file_descriptor_usb_t {
    usb_handler_t* handler;
    const char* port;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    void set_baudrate(uint32_t baud);
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

static usb_handler_t usb_handler {};
file_descriptor_usb_t file_descriptor_usb_instance {&usb_handler, "/usb", nullptr, 0};

void file_descriptor_usb_t::init() {
    osSemaphoreAttr_t attr = {};
    attr.cb_mem = &handler->usb_read_sem_cb;
    attr.cb_size = sizeof(handler->usb_read_sem_cb);
    handler->usb_read_sem = osSemaphoreNew(1, 1, &attr);

    attr.cb_mem = &handler->usb_write_sem_cb;
    attr.cb_size = sizeof(handler->usb_write_sem_cb);
    handler->usb_write_sem = osSemaphoreNew(1, 1, &attr);
}

void file_descriptor_usb_t::set_baudrate(uint32_t ) {}

auto file_descriptor_usb_t::read(uint32_t tout) -> Result<std::vector<uint8_t>> {
    osThreadFlagsSet(handler->usb_read_thd, 0b10); // cancel the awaiting thread
    handler->usb_read_thd = osThreadGetId();

    // read until available
    auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
    if (flag & osFlagsError) {
        handler->usb_read_thd = nullptr;
        osSemaphoreRelease(handler->usb_read_sem);
        return Err(Error::TransferTimeout);
    }
    if (flag & 0b10) {
        return Err(Error{static_cast<int>(flag), "canceled"});
    }

    handler->usb_read_thd = nullptr;
    osSemaphoreRelease(handler->usb_read_sem);

    if (etl::heap::freeSize < received_data_len)
        return Err(Error{-1, "No memory"});

    return Ok(std::vector<uint8_t>(received_data, received_data + received_data_len));
}

auto file_descriptor_usb_t::read_until(uint32_t tout, size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    std::vector<uint8_t> buffer(n);
    osThreadFlagsSet(handler->usb_read_thd, 0b10); // cancel the awaiting thread
    handler->usb_read_thd = osThreadGetId();

    auto start = etl::time::now();
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (etl::time::elapsed(start).tick < tout) {
        auto flag = osThreadFlagsWait(0b11, osFlagsWaitAny, tout);
        if (flag & osFlagsError) {
            handler->usb_read_thd = nullptr;
            osSemaphoreRelease(handler->usb_read_sem);
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
            handler->usb_read_thd = nullptr;
            osSemaphoreRelease(handler->usb_read_sem);
            return Ok(std::move(buffer));
        }
    }

    handler->usb_read_thd = nullptr;
    osSemaphoreRelease(handler->usb_read_sem);
    return Err(Error::TransferTimeout);
}

auto file_descriptor_usb_t::write(uint32_t, std::string_view data) -> Result<void> {
    static size_t last_data_size;
    osSemaphoreAcquire(handler->usb_write_sem, last_data_size); // assuming the speed is 1 byte / ms
    if (auto res = CDC_Transmit_FS((uint8_t*)data.data(), data.size()); res != USBD_OK) {
        return Err(Error{res, "USBD busy"});
    } 
    last_data_size = data.size();
    return Ok();
}

auto file_descriptor_usb_t::wait_until_ready(uint32_t tout) -> Result<void> {
    if (osSemaphoreAcquire(handler->usb_read_sem, tout) != osOK) {
        return Err(Error::TransferTimeout);
    }
    return Ok();
}

extern "C" void CDC_ReceiveCplt_Callback(const uint8_t *pbuf, uint32_t len) {
    file_descriptor_usb_instance.received_data = pbuf;
    file_descriptor_usb_instance.received_data_len = len;
    osThreadFlagsSet(usb_handler.usb_read_thd, 0b1);
}
extern "C" void CDC_TransmitCplt_Callback(const uint8_t *pbuf, uint32_t len) {
    UNUSED(pbuf);
    UNUSED(len);
    osSemaphoreRelease(usb_handler.usb_write_sem);
}
#endif