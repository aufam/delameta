#if defined(USE_HAL_DRIVER) // STM32 project must define USE_HAL_DRIVER
#include "cmsis_os2.h"

auto delameta_detail_get_time_stamp() {
    return osKernelGetTickCount();
}

auto delameta_detail_count_ms(decltype(delameta_detail_get_time_stamp()) start) {
    return osKernelGetTickCount() - start;
}

template <typename F>
void delameta_detail_on_sigint(F&&) {
    // do nothing
}

#else
#include <chrono>
#include <functional>
#include <csignal>

auto delameta_detail_get_time_stamp() {
    return std::chrono::high_resolution_clock::now();
}

auto delameta_detail_count_ms(decltype(delameta_detail_get_time_stamp()) start) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

template <typename F>
void delameta_detail_on_sigint(F&& fn) {
    static std::function<void()> at_sigint;
    at_sigint = std::move(fn);
    ::signal(SIGINT, +[](int) { at_sigint(); });
}
#endif
