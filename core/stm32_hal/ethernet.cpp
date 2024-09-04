#ifndef DELAMETA_STM32_DISABLE_SOCKET

#include "main.h"
#include "socket.h"
#include "delameta/debug.h"
#include "delameta/error.h"
#include <etl/async.h>
#include <etl/heap.h>
#include <cstring>
#include <FreeRTOS.h>
#include <etl/mutex.h>

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

static wiz_NetInfo wizchip_net_info = { 
    .mac={0x00, 0x08, 0xdc, 0xff, 0xee, 0xdd},
    .ip={10, 20, 30, 2},
    .sn={255, 255, 255, 0},
    .gw={10, 20, 30, 1},
    .dns={10, 20, 30, 1},
    .dhcp=NETINFO_STATIC,
}; 

bool delameta_wizchip_is_setup = false;

using namespace Project;
using namespace Project::delameta;
using namespace etl::literals;

using etl::Err;
using etl::Ok;

static void check_phy_link() {
    while (wizphy_getphylink() == PHY_LINK_OFF) {
        DBG(warning, "PHY_LINK_OFF");
        etl::task::sleep(50ms).await();
    }
};

struct socket_descriptor_t {
    bool is_busy;
};
socket_descriptor_t delameta_wizchip_socket_descriptors[_WIZCHIP_SOCK_NUM_];

struct addrinfo {
    uint8_t ip[4];
    uint16_t port;
};
auto delameta_detail_resolve_domain(const std::string& domain) -> Result<addrinfo>;

static etl::Mutex mtx;

auto delameta_wizchip_socket_status() -> std::string {
    std::string res;
    res.reserve(_WIZCHIP_SOCK_NUM_ * 8);
    res += "{";
    for (auto &desc : delameta_wizchip_socket_descriptors) {
        res += desc.is_busy ? "true" : "false";
        res += ",";
    }
    res.back() = '}';
    return res;
}

auto delameta_wizchip_socket_open(uint8_t protocol, int port, int flag) -> Result<int> {
    if (!delameta_wizchip_is_setup) {
        etl::time::sleep(100ms);
    }

    auto lock = mtx.lock().await();

    for (auto i: etl::range(_WIZCHIP_SOCK_NUM_)) if (!delameta_wizchip_socket_descriptors[i].is_busy) {
        auto res = ::socket(i, protocol, port, flag);
        if (res < 0) {
            return Err(Error{res, "socket"});
        } else {
            delameta_wizchip_socket_descriptors[i].is_busy = true;
            return Ok(i);
        }
    }

    return Err(Error{-1, "no socket"});
}

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

    if (!delameta_wizchip_is_setup) return;

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

    mtx.init();

    etl::async([]() {
        auto lock = mtx.lock().await();

        etl::task::sleep(100ms).await();
        HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_RESET);
        etl::task::sleep(100ms).await();
        HAL_GPIO_WritePin(DELAMETA_STM32_WIZCHIP_CS_PORT, DELAMETA_STM32_WIZCHIP_CS_PIN, GPIO_PIN_SET);
        etl::task::sleep(100ms).await();
    
        DBG(info, "ethernet is starting...");

        uint8_t memsize[2][8] = { {2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2} };
        if (wizchip_init(memsize[0], memsize[1]) == -1) {
            DBG(warning, "wizchip_init fail");
            return;
        }

        check_phy_link();
        delameta_wizchip_is_setup = true;
        delameta_stm32_hal_wizchip_set_net_info(nullptr, nullptr, nullptr, nullptr, nullptr);
    });
}

#endif