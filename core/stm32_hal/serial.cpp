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

struct serial_descriptor_uart_t {
    uart_handler_t* handler;
    const char* port;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<void> set_baudrate(uint32_t baud);
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

#ifdef DELAMETA_STM32_USE_HAL_UART1
extern serial_descriptor_uart_t serial_descriptor_uart_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART2
extern serial_descriptor_uart_t serial_descriptor_uart_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART3
extern serial_descriptor_uart_t serial_descriptor_uart_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART4
extern serial_descriptor_uart_t serial_descriptor_uart_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART5
extern serial_descriptor_uart_t serial_descriptor_uart_instance5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART6
extern serial_descriptor_uart_t serial_descriptor_uart_instance6;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART7
extern serial_descriptor_uart_t serial_descriptor_uart_instance7;
#endif
#ifdef DELAMETA_STM32_USE_HAL_UART8
extern serial_descriptor_uart_t serial_descriptor_uart_instance8;
#endif

#endif

#ifdef DELAMETA_STM32_HAS_I2C
struct serial_descriptor_i2c_t {
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
extern serial_descriptor_i2c_t serial_descriptor_i2c_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C2
extern serial_descriptor_i2c_t serial_descriptor_i2c_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C3
extern serial_descriptor_i2c_t serial_descriptor_i2c_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C4
extern serial_descriptor_i2c_t serial_descriptor_i2c_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_I2C5
extern serial_descriptor_i2c_t serial_descriptor_i2c_instance5;
#endif

#endif

#ifdef DELAMETA_STM32_HAS_CAN
struct serial_descriptor_can_t {
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

extern serial_descriptor_can_t serial_descriptor_can_instance;

#endif

#ifdef DELAMETA_STM32_HAS_SPI
struct serial_descriptor_spi_t {
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
extern serial_descriptor_spi_t serial_descriptor_spi_instance1;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI2
extern serial_descriptor_spi_t serial_descriptor_spi_instance2;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI3
extern serial_descriptor_spi_t serial_descriptor_spi_instance3;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI4
extern serial_descriptor_spi_t serial_descriptor_spi_instance4;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI5
extern serial_descriptor_spi_t serial_descriptor_spi_instance5;
#endif
#ifdef DELAMETA_STM32_USE_HAL_SPI6
extern serial_descriptor_spi_t serial_descriptor_spi_instance6;
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

struct serial_descriptor_usb_t {
    usb_handler_t* handler;
    const char* port;
    const uint8_t* received_data;
    size_t received_data_len;

    void init();
    Result<void> set_baudrate(uint32_t baud);
    Result<std::vector<uint8_t>> read(uint32_t tout);
    Result<std::vector<uint8_t>> read_until(uint32_t tout, size_t n);
    Result<void> write(uint32_t tout, std::string_view data);
    Result<void> wait_until_ready(uint32_t tout);
};

extern serial_descriptor_usb_t serial_descriptor_usb_instance;

#endif

struct serial_descriptor_dummy_t {
    const char* port;

    void init() {}
    Result<void> set_baudrate(uint32_t) { return Err(Error{-1, "no impl"}); }
    Result<std::vector<uint8_t>> read(uint32_t) { return Err(Error{-1, "no impl"}); }
    Result<std::vector<uint8_t>> read_until(uint32_t, size_t) { return Err(Error{-1, "no impl"}); }
    Result<void> write(uint32_t, std::string_view) { return Err(Error{-1, "no impl"}); }
    Result<void> wait_until_ready(uint32_t) { return Err(Error{-1, "no impl"}); }
};

using serial_descriptor_t = std::variant<serial_descriptor_dummy_t*
    #ifdef DELAMETA_STM32_HAS_UART
    , serial_descriptor_uart_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_I2C
    , serial_descriptor_i2c_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_SPI
    , serial_descriptor_spi_t*
    #endif
    #ifdef DELAMETA_STM32_HAS_CAN
    , serial_descriptor_can_t*
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    , serial_descriptor_usb_t*
    #endif
>;

static serial_descriptor_t serial_descriptors[] = {
    #ifdef DELAMETA_STM32_USE_HAL_UART1
    &serial_descriptor_uart_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART2
    &serial_descriptor_uart_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART3
    &serial_descriptor_uart_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART4
    &serial_descriptor_uart_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART5
    &serial_descriptor_uart_instance5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART6
    &serial_descriptor_uart_instance6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART7
    &serial_descriptor_uart_instance7,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_UART8
    &serial_descriptor_uart_instance8,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C1
    &serial_descriptor_i2c_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C2
    &serial_descriptor_i2c_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C3
    &serial_descriptor_i2c_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C4
    &serial_descriptor_i2c_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_I2C5
    &serial_descriptor_i2c_instance5,
    #endif
    #ifdef DELAMETA_STM32_HAS_CAN
    &serial_descriptor_can_instance,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI1
    &serial_descriptor_spi_instance1,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI2
    &serial_descriptor_spi_instance2,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI3
    &serial_descriptor_spi_instance3,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI4
    &serial_descriptor_spi_instance4,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI5
    &serial_descriptor_spi_instance5,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_SPI6
    &serial_descriptor_spi_instance6,
    #endif
    #ifdef DELAMETA_STM32_USE_HAL_USB
    &serial_descriptor_usb_instance,
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
    for (size_t i = 0; i < std::size(serial_descriptors); ++i) {
        bool found = false;
        Error err{HAL_OK, ""};
        std::visit([&](auto* fd){
            if (!found && fd->port == args.port) { 
                auto set_result = fd->set_baudrate(args.baud);
                if (set_result.is_err()) {
                    err = std::move(set_result.unwrap_err()); 
                }
                found = true;
            }
        }, serial_descriptors[i]);

        if (found) {
            if (err.code == HAL_OK) {
                return Ok(Serial(file, line, i, args.timeout));
            } else {
                return Err(std::move(err));
            }
        }
    }

    return Err(Error{-1, "not found"});
}

auto Serial::read() -> Result<std::vector<uint8_t>> {
    if (fd < 0 || fd >= (int)std::size(serial_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->read(tout); }, serial_descriptors[fd]);
}

auto Serial::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (fd < 0 || fd >= (int)std::size(serial_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->read_until(tout, n); }, serial_descriptors[fd]);
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
    if (fd < 0 || fd >= (int)std::size(serial_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->write(tout, data); }, serial_descriptors[fd]);
}

auto Serial::wait_until_ready() -> Result<void> {
    if (fd < 0 || fd >= (int)std::size(serial_descriptors))
        return Err(Error{-1, "Invalid fd"});

    uint32_t tout = timeout > 0 ? timeout : osWaitForever;
    return std::visit([&](auto* fd) { return fd->wait_until_ready(tout); }, serial_descriptors[fd]);
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

// peripheral init
extern "C" void delameta_stm32_hal_init() {
    for (auto& fd: serial_descriptors) {
        std::visit([](auto* fd) { fd->init(); }, fd);
    }
}