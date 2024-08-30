#if defined(USE_HAL_DRIVER)
#include "cmsis_os2.h"

auto delameta_detail_get_time_stamp() {
    return osKernelGetTickCount();
}

auto delameta_detail_count_ms(decltype(delameta_detail_get_time_stamp()) start) {
    return osKernelGetTickCount() - start;
}

#else
#include <chrono>

auto delameta_detail_get_time_stamp() {
    return std::chrono::high_resolution_clock::now();
}

auto delameta_detail_count_ms(decltype(delameta_detail_get_time_stamp()) start) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}
#endif
