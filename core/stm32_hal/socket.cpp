#ifndef DELAMETA_STM32_DISABLE_SOCKET

#include "main.h"
#include "socket.h"
#include "delameta/socket.h"
#include "delameta/debug.h"
#include "etl/async.h"
#include "etl/heap.h"
#include <cstring>


#ifndef DELAMETA_STM32_WIZCHIP_CS_PORT
#error "DELAMETA_STM32_WIZCHIP_CS_PORT is not defined"
#endif
#ifndef DELAMETA_STM32_WIZCHIP_CS_PIN
#error "DELAMETA_STM32_WIZCHIP_CS_PIN is not defined"
#endif
#ifndef DELAMETA_STM32_WIZCHIP_RST_PORT
#error "DELAMETA_STM32_WIZCHIP_RST_PORT is not defined"
#endif
#ifndef DELAMETA_STM32_WIZCHIP_RST_PIN
#error "DELAMETA_STM32_WIZCHIP_RST_PIN is not defined"
#endif
#ifndef DELAMETA_STM32_WIZCHIP_SPI
#error "DELAMETA_STM32_WIZCHIP_SPI is not defined"
#endif

extern SPI_HandleTypeDef DELAMETA_STM32_WIZCHIP_SPI;
extern SPI_HandleTypeDef DELAMETA_STM32_WIZCHIP_SPI;

static wiz_NetInfo wizchip_net_info = { 
    .mac={0x00, 0x08, 0xdc, 0xff, 0xee, 0xdd},
    .ip={10, 20, 30, 2},
    .sn={255, 255, 255, 0},
    .gw={10, 20, 30, 1},
    .dns={10, 20, 30, 1},
    .dhcp=NETINFO_STATIC,
}; 

static bool wizchip_is_setup = false;

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

using etl::Err;
using etl::Ok;

struct socket_descriptor_t {
    bool is_busy;
};

static socket_descriptor_t socket_descriptors[_WIZCHIP_SOCK_NUM_];

static void check_phy_link() {
    while (wizphy_getphylink() == PHY_LINK_OFF) {
        DBG(warning, "PHY_LINK_OFF");
        etl::task::sleep(50ms).await();
    }
};

extern "C" void delameta_stm32_hal_wizchip_set_net_info(
    const uint8_t mac[6], 
    const uint8_t ip[4], 
    const uint8_t sn[4], 
    const uint8_t gw[4], 
    const uint8_t dns[4]
) {
    if (mac) ::memcpy(wizchip_net_info.mac, mac, 6);
    if (ip) ::memcpy(wizchip_net_info.ip, ip, 4);
    if (gw) ::memcpy(wizchip_net_info.sn, sn, 4);
    if (dns) ::memcpy(wizchip_net_info.dns, dns, 4);

    if (!wizchip_is_setup) return;

	wiz_PhyConf phyConf;
	wizphy_getphystat(&phyConf);

    wiz_NetTimeout tout = {.retry_cnt=10, .time_100us=100};

	wizchip_setnetinfo(&wizchip_net_info);
	wizchip_getnetinfo(&wizchip_net_info);

	ctlnetwork(CN_SET_TIMEOUT,(void*)&tout);
	ctlnetwork(CN_GET_TIMEOUT, (void*)&tout);

    DBG_VAL(info, phyConf.by);
    DBG_VAL(info, phyConf.mode);
    DBG_VAL(info, phyConf.speed);
    DBG_VAL(info, phyConf.duplex);
    DBG_VAL(info, phyConf.duplex);
    DBG_VAL(info, tout.retry_cnt);
    DBG_VAL(info, tout.time_100us * 10);

    static char buffer[64];
    snprintf(buffer, 64, "dhcp: %s", wizchip_net_info.dhcp == NETINFO_STATIC ? "static" : "dynamic");
    DBG(info, buffer);
    snprintf(buffer, 64, "dns: %d.%d.%d.%d", wizchip_net_info.dns[0], wizchip_net_info.dns[1], wizchip_net_info.dns[2], wizchip_net_info.dns[3]);
    DBG(info, buffer);
    snprintf(buffer, 64, "mac: %02x:%02x:%02x:%02x:%02x:%02x", wizchip_net_info.mac[0], wizchip_net_info.mac[1], wizchip_net_info.mac[2], wizchip_net_info.mac[3], wizchip_net_info.mac[4], wizchip_net_info.mac[5]);
    DBG(info, buffer);
    snprintf(buffer, 64, "gw: %d.%d.%d.%d", wizchip_net_info.gw[0], wizchip_net_info.gw[1], wizchip_net_info.gw[2], wizchip_net_info.gw[3]);
    DBG(info, buffer);
    snprintf(buffer, 64, "ip: %d.%d.%d.%d", wizchip_net_info.ip[0], wizchip_net_info.ip[1], wizchip_net_info.ip[2], wizchip_net_info.ip[3]);
    DBG(info, buffer);
}

extern "C" void delameta_stm32_hal_wizchip_init() {
    HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_RST_PORT, DELAMETA_STM32_WIZCHIP_RST_PIN, GPIO_PIN_SET);

    reg_wizchip_cs_cbfunc(
        [] { HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_RESET); }, 
        [] { HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_SET); }
    );
    reg_wizchip_spi_cbfunc(
        [] {
            uint8_t byte; 
            HAL_SPI_Receive(&DELAMETA_STM32_WIZCHIP_SPI, &byte, 1, HAL_MAX_DELAY); 
            return byte;
        }, 
        [] (uint8_t byte) { 
            HAL_SPI_Transmit(&DELAMETA_STM32_WIZCHIP_SPI, &byte, 1, HAL_MAX_DELAY); 
        }
    );
    reg_wizchip_spiburst_cbfunc(
        [] (uint8_t* buf, uint16_t len) { 
            HAL_SPI_Receive(&DELAMETA_STM32_WIZCHIP_SPI, buf, len, HAL_MAX_DELAY); 
        }, 
        [] (uint8_t* buf, uint16_t len) { 
            HAL_SPI_Transmit(&DELAMETA_STM32_WIZCHIP_SPI, buf, len, HAL_MAX_DELAY); 
        }
    );

    etl::async([]() {
        etl::task::sleep(100ms).await();
        HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_RESET);
        etl::task::sleep(100ms).await();
        HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_SET);
        etl::task::sleep(100ms).await();
    
        DBG(info, "ethernet start");

        uint8_t memsize[2][8] = { {2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2} };
        if (wizchip_init(memsize[0], memsize[1]) == -1) {
            DBG(warning, "wizchip_init fail");
            return;
        }

        check_phy_link();
        wizchip_is_setup = true;
        delameta_stm32_hal_wizchip_set_net_info(nullptr, nullptr, nullptr, nullptr, nullptr);
    });
}

auto Socket::New(const char* file, int line, int protocol, int port, int flag) -> Result<Socket> {
    while (!wizchip_is_setup) {
        etl::time::sleep(100ms);
    }
    for (auto i: etl::range(_WIZCHIP_SOCK_NUM_)) if (!socket_descriptors[i].is_busy) {
        auto res = ::socket(i, protocol, port, flag);
        if (res < 0) {
            return Err(Error{res, "socket"});
        } else {
            return Ok(Socket(file, line, i));
        }
    }

    return Err(Error{-1, "no socket"});
}

Socket::Socket(const char* file, int line, int socket) 
    : socket(socket)
    , keep_alive(true)
    , timeout(-1)
    , max(-1) 
    , file(file)
    , line(line) {}

Socket::Socket(Socket&& other) 
    : socket(std::exchange(other.socket, -1))
    , keep_alive(other.keep_alive)
    , timeout(other.timeout)
    , max(other.max) 
    , file(other.file)
    , line(other.line) {}

Socket::~Socket() {
    if (socket >= 0) {
        ::close(socket);
        socket_descriptors[socket].is_busy = false;
        socket = -1;
    }
}

auto Socket::read() -> Result<std::vector<uint8_t>> {
    auto start = etl::time::now();
    while (true) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        size_t len = ::getSn_RX_RSR(socket);
        if (len == 0) {
            if (timeout > 0 && etl::time::elapsed(start) > etl::time::seconds(timeout)) {
                return Err(Error::TransferTimeout);
            }
            etl::time::sleep(10ms);
            continue;
        }

        if (etl::heap::freeSize < len) {
            return Err(Error{-1, "No memory"});
        }

        auto res = std::vector<uint8_t>(len);
        ::recv(socket, res.data(), len);
        return Ok(std::move(res));
    }
}

auto Socket::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (n == 0 || etl::heap::freeSize < n) {
        return Err(Error{-1, "No memory"});
    }

    auto start = etl::time::now();
    std::vector<uint8_t> buffer(n);
    size_t remaining_size = n;
    auto ptr = buffer.data();

    while (true) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        size_t len = ::getSn_RX_RSR(socket);
        if (len == 0) {
            if (timeout > 0 && etl::time::elapsed(start) > etl::time::seconds(timeout)) {
                return Err(Error::TransferTimeout);
            }
            etl::time::sleep(10ms);
            continue;
        }

        auto size = std::min(remaining_size, len);
        ::recv(socket, ptr, size);    
        remaining_size -= size;

        if (remaining_size == 0) {
            return Ok(std::move(buffer));
        }
    }
}

auto Socket::read_as_stream(size_t n) -> Stream {
    Stream s;
    for (int total = n; total > 0;) {
        int size = std::min(total, 2048);
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

auto Socket::write(std::string_view data) -> Result<void> {
    size_t total = 0;
    for (size_t i = 0; i < data.size();) {
        int stat = getSn_SR(socket);
        if (stat != SOCK_ESTABLISHED) {
            return Err(Error{stat, "Closed by peer"});
        }

        auto n = std::min<size_t>(2048, data.size() - i);
        auto sent = ::send(socket, (uint8_t*)&data[i], n);
        
        if (sent == 0) {
            return Err(Error::ConnectionClosed);
        } else if (sent < 0) {
            return Err(Error{sent, "socket write"});
        }

        total += sent;
        i += sent;
    }

    return Ok();
}

#endif