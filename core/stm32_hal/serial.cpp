#include "main.h" // from Core/Src

#ifdef DELAMETA_STM32_USE_HAL_USB
#include "usbd_cdc_if.h"
#endif

#include "delameta/serial.h"
#include "delameta/debug.h"
#include "etl/time.h"
#include "etl/heap.h"

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

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
    const char* port;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_UART1
extern file_descriptor_uart_t file_descriptor_uart_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART2
extern file_descriptor_uart_t file_descriptor_uart_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART3
extern file_descriptor_uart_t file_descriptor_uart_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART4
extern file_descriptor_uart_t file_descriptor_uart_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART5
extern file_descriptor_uart_t file_descriptor_uart_instance5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART6
extern file_descriptor_uart_t file_descriptor_uart_instance6;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART7
extern file_descriptor_uart_t file_descriptor_uart_instance7;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART8
extern file_descriptor_uart_t file_descriptor_uart_instance8;
#endif

#endif

#ifdef DELAMETA_STM32_HAS_I2C
struct file_descriptor_i2c_t {
    I2C_HandleTypeDef* handler;
    const char* __file;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_I2C1
extern file_descriptor_i2c_t file_descriptor_i2c_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C2
extern file_descriptor_i2c_t file_descriptor_i2c_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C3
extern file_descriptor_i2c_t file_descriptor_i2c_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C4
extern file_descriptor_i2c_t file_descriptor_i2c_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C5
extern file_descriptor_i2c_t file_descriptor_i2c_instance5;
#endif

#endif

#ifdef DELAMETA_STM32_HAS_CAN
struct file_descriptor_can_t {
    CAN_HandleTypeDef* handler;
    const char* __file;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

extern file_descriptor_can_t file_descriptor_can_instance;

#endif

#ifdef DELAMETA_STM32_HAS_SPI
struct file_descriptor_spi_t {
    SPI_HandleTypeDef* handler;
    const char* __file;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_SPI1
extern file_descriptor_spi_t file_descriptor_spi_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI2
extern file_descriptor_spi_t file_descriptor_spi_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI3
extern file_descriptor_spi_t file_descriptor_spi_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI4
extern file_descriptor_spi_t file_descriptor_spi_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI5
extern file_descriptor_spi_t file_descriptor_spi_instance5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI6
extern file_descriptor_spi_t file_descriptor_spi_instance6;
#endif

#endif

#ifdef DELAMETA_STM32_USE_HAL_USB
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
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

extern file_descriptor_usb_t file_descriptor_usb_instance;

#endif

struct file_descriptor_dummy_t {
    const char* port;

    void init() {}
    void set_baudrate(uint32_t) {}
    Result<std::vector<uint8_t>> read(uint32_t) { return Err(Error{-1, "No impl"}); }
    Result<std::vector<uint8_t>> read_until(uint32_t, size_t) { return Err(Error{-1, "No impl"}); }
    Result<void> write(uint32_t, std::string_view) { return Err(Error{-1, "No impl"}); }
    Result<void> wait_until_ready(uint32_t) { return Err(Error{-1, "No impl"}); }
};

using file_descriptor_t = std::variant<file_descriptor_dummy_t*
    #ifdef DELAMETA_STM32_HAS_UART
    , file_descriptor_uart_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_I2C
    , file_descriptor_i2c_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_SPI
    , file_descriptor_spi_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_CAN
    , file_descriptor_can_t*
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    , file_descriptor_usb_t*
    #endif
>;

static file_descriptor_t file_descriptors[] = {
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    &file_descriptor_uart_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    &file_descriptor_uart_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    &file_descriptor_uart_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    &file_descriptor_uart_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    &file_descriptor_uart_instance5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    &file_descriptor_uart_instance6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    &file_descriptor_uart_instance7,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    &file_descriptor_uart_instance8,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    &file_descriptor_i2c_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    &file_descriptor_i2c_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    &file_descriptor_i2c_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    &file_descriptor_i2c_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    &file_descriptor_i2c_instance5,
    #endif
    #ifdef DELAMETA_STM32_HAS_CAN
    &file_descriptor_can_instance,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    &file_descriptor_spi_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    &file_descriptor_spi_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    &file_descriptor_spi_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    &file_descriptor_spi_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    &file_descriptor_spi_instance5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    &file_descriptor_spi_instance6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    &file_descriptor_usb_instance,
    #endif
};

Serial::Serial(const char* file, int line, int fd, int timeout)
    : Descriptor()
    , StreamSessionClient(this)
    , fd(fd)
    , timeout(timeout)
    , file(file)
    , line(line) {}

Serial::Serial(Serial&& other) 
    : Descriptor()
    , StreamSessionClient(this)
    , fd(std::exchange(other.fd, -1))
    , timeout(other.timeout)
    , file(other.file)
    , line(other.line) {}

Serial::~Serial() {
    if (fd >= 0) {
        fd = -1;
    }
}

auto Serial::Open(const char* file, int line, Args args) -> Result<Serial> {
    for (size_t i = 0; i < std::size(file_descriptors); ++i) {
        bool found = false;
        std::visit([&](auto* fd){
            if (!found && fd->port == args.port) { 
                // fd->set_baudrate(args.baud);
                found = true;
            }
        }, file_descriptors[i]);

        if (found) {
            return Ok(Serial(file, line, i, args.timeout));
        }
    }

    return Err(Error{-1, "no __file"});
}

auto Serial::read() -> Result<std::vector<uint8_t>> {
    if (fd < 0 || fd >= (int)std::size(file_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->read(tout); }, file_descriptors[fd]);
}

auto Serial::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (fd < 0 || fd >= (int)std::size(file_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->read_until(tout, n); }, file_descriptors[fd]);
}

auto Serial::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, 128);
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

auto Serial::write(std::string_view data) -> Result<void> {
    if (fd < 0 || fd >= (int)std::size(file_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->write(tout, data); }, file_descriptors[fd]);
}

auto Serial::wait_until_ready() -> Result<void> {
    if (fd < 0 || fd >= (int)std::size(file_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->wait_until_ready(tout); }, file_descriptors[fd]);
}

auto Server<Serial>::start(const char* file, int line, Serial::Args args) -> Result<void> {
    auto [ser, ser_err] = Serial::Open(file, line, std::move(args));
    if (ser_err) return Err(std::move(*ser_err));

    bool is_running {true};
    on_stop = [this, &is_running]() { 
        is_running = false;
        on_stop = {};
    };

    while (is_running) {
        auto wait_result = ser->wait_until_ready();
        if (wait_result.is_err()) continue;

        auto read_result = ser->read();
        if (read_result.is_err()) continue;

        auto stream = execute_stream_session(*ser, "Serial", read_result.unwrap());
        stream >> *ser;
    }

    stop();
    return Ok();
}

void Server<Serial>::stop() {
    if (on_stop) {
        on_stop();
    }
}

// peripheral init init
extern "C" void delameta_stm32_hal_init() {
    for (auto& fd: file_descriptors) {
        std::visit([](auto* fd) { fd->init(); }, fd);
    }
}