## <a id="introduction"></a>Delameta
C++ framework for socket and serial programming for Linux and STM32

## <a id="table_of_content"></a>Table of Contents

- [Introduction](#introduction)
- [Table of Content](#table_of_content)
- [Project Structures](#project_structures)
- [How to Use](#how_to_use)
- [Code Considerations and Structure](#code_consideration)
- [Features](#fetures)

## <a id="project_structures"></a>Project Structure
    .
    ├── CMakeLists.txt              # Build configuration
    ├── README.md                   # Project documentation
    ├── app/                        # Example demo app
    ├── include/delameta/           # Header files directory
    ├── core/
    │ ├── linux/                    # Source files for linux
    │ ├── stm32_hal/                # Source files for STM32
    ├── src/                        # Source files
    ├── test/                       # Unit testing

## <a id="how_to_use"></a>How to Use
You can add this library as an external dependency in your CMake configuration as follows:
```cmake
include(FetchContent)

set(DELAMETA_BUILD_APP OFF CACHE BOOL "Disable build app" FORCE)
set(DELAMETA_BUILD_TEST OFF CACHE BOOL "Disable build test" FORCE)

FetchContent_Declare(
    delameta
    GIT_REPOSITORY https://github.com/aufam/delameta.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(delameta)
target_link_libraries(your_awesome_project delameta)
```
Or you can add this library as a Git submodule:
```bash
git submodule add https://github.com/aufam/delameta.git path/to/your/submodules
git submodule update --init --recursive
```
Then modify your CMake configuration as follows:
```cmake
set(DELAMETA_BUILD_APP OFF CACHE BOOL "Disable build app" FORCE)
set(DELAMETA_BUILD_TEST OFF CACHE BOOL "Disable build test" FORCE)

add_subdirectory(path/to/your/submodules/delameta)
target_link_libraries(your_awesome_project delameta)
```

If your project is for STM32, you need to enable this flag:

```cmake
set(DELAMETA_TARGET_LINUX OFF CACHE BOOL "Disable target for Linux" FORCE)
set(DELAMETA_TARGET_STM32 ON CACHE BOOL "Enable target for STM32" FORCE)
```

### Requirements
* c++17 minimum

### Linux requirements
* libssl
  ```bash
  sudo apt install libssl-dev
  ```

### STM32 Requirements
Typically, you would setup your STM32 configuration using STM32CubeMX or STM32CubeIDE. Here are the requirements:
* Enable HAL driver
* Enable FreeRTOS with interface CMSIS_V2
* Enable DMA setting for UART receive if using UART
* Enable SPI for socket programming if using Wizchip

Here are some optional macros that has to be defined in order to link this library to certain peripherals:
* `DELAMETA_STM32_USE_HAL_UARTx` where `x` is the UART number
* `DELAMETA_STM32_USE_HAL_I2Cx` where `x` is the I2C number
* `DELAMETA_STM32_USE_HAL_SPIx` where `x` is the SPI number
* `DELAMETA_STM32_USE_HAL_CANx` where `x` is the CAN number
* `DELAMETA_STM32_HAL_CAN_USE_FIFOx` where `x` is `0` or `1`
* `DELAMETA_STM32_USE_HAL_USB` for USB CDC
* `DELAMETA_STM32_WIZCHIP_SPI=hspix` where `x` is the SPI number
* `DELAMETA_STM32_WIZCHIP_CS_PORT=GPIOx` where `x` is the GPIO port
* `DELAMETA_STM32_WIZCHIP_CS_PIN=GPIO_Pin_x` where `x` is the GPIO pin number
* `DELAMETA_STM32_WIZCHIP_RST_PORT=GPIOx` where `x` is the GPIO port
* `DELAMETA_STM32_WIZCHIP_RST_PIN=GPIO_Pin_x` where `x` is the GPIO pin number

Socket programming relies on `etl::tasks` with utilizes FreeRTOS threads.
Here are some example macros to configure `etl::tasks`:
* `ETL_ASYNC_N_CHANNELS=8` allocate 8 threads as asynchronous tasks
* `ETL_ASYNC_TASK_THREAD_SIZE=2048` allocates 2048 words for each thread
* `ETL_ASYNC_TASK_SENDER_MEMPOOL_SIZE=32` allocates 32 bytes for each thread's argument memory pool

#### Some Initialization
Add the following initialization functions to your main function:
```C
void etl_tasks_init(void);
void delameta_stm32_hal_init(void);
void delameta_stm32_hal_wizchip_init(void);
void delameta_stm32_hal_wizchip_set_net_info(
    const uint8_t mac[6], 
    const uint8_t ip[4], 
    const uint8_t sn[4], 
    const uint8_t gw[4], 
    const uint8_t dns[4]
);

int main() {
  ...

  /* USER CODE BEGIN 2 */
  etl_tasks_init();
  delameta_stm32_hal_init();
  delameta_stm32_hal_wizchip_set_net_info(\*your configuration*\);
  delameta_stm32_hal_wizchip_init();
  /* USER CODE END 2 */

  ...
}
```
#### USB CDC Setup
Since CubeMX does not provide customizable transmit and receive complete callbacks, you will need to manually add them in the `USB_DEVICE/App/usb_cdc_if.c` file:
```C
...

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
__weak void CDC_ReceiveCplt_Callback(const uint8_t *pbuf, uint32_t len) {
  UNUSED(pbuf);
  UNUSED(len);
}
__weak void CDC_TransmitCplt_Callback(const uint8_t *pbuf, uint32_t len) {
  UNUSED(pbuf);
  UNUSED(len);
}
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

...

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  CDC_ReceiveCplt_Callback(Buf, *Len);
  return (USBD_OK);
  /* USER CODE END 6 */
}

...

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 13 */
  UNUSED(Buf);
  UNUSED(Len);
  UNUSED(epnum);
  CDC_TransmitCplt_Callback(Buf, *Len);
  /* USER CODE END 13 */
  return result;
}
```

For a more complete example of how to use this library with STM32, see my template project for the [STM32 blackpill](https://github.com/aufam/blackpill.git) 

## <a id="code_consideration"></a>Code Considerations and Structure

### Dependency
The only dependency it has is [`etl`](https://github.com/aufam/etl.git).
It uses [`etl::Result`](https://github.com/aufam/etl/blob/main/include/etl/result.h) to avoid exception.
It also uses lightweight json parser [`etl::Json`](https://github.com/aufam/etl/blob/main/include/etl/json.h). 
The cmake will check for the availability of `etl` target, if it's already available in your project configuration, 
it will use it, otherwise it will fetch from the GitHub repository

### Global namespace
All C++ code, including the `delameta` and `etl` namespaces, is enclosed within a global `Project` namespace 
in order to avoid shadowing to C's code base, 
or conflicting namespace since there is already more established 
[Embedded Template Library (ETL)](https://github.com/ETLCPP/etl).
Therefore, you'll need to add using namespace Project; in your C++ files.

### Global headers and source files
No OS specific headers are included in the `include/delameta` or `src` folders. OS specific source codes are
only in the `core/linux` and `core/stm32_hal` directories.

## <a id="fetures"></a>Features

### Dynamic Memory Management for STM32
The `etl` library has custom implementations of `::malloc()`, `::calloc()`, `::realloc()`, `::free()`, `::new`, `::new[]`, 
and `::delete` which are tailored to work with FreeRTOS dynamic memory management. 
This ensures that using dynamic containers in STM32 projects is safe and compatible with the FreeRTOS environment.

### Result Template Type with [etl::Result](https://github.com/aufam/etl/blob/main/include/etl/result.h)
`etl::Result<T, E>` is an `std::variant` that holds a value of `etl::Ok<T>` or `etl::Err<E>` similar to rust's `Result`.

Example usage:
* Unwrap result or error value:
  ```c++
  auto foo() -> Result<int, std::string> {
    return Ok(42);
  }

  auto bar() -> Result<int, std::string> {
    return Err("Not implemented");
  }

  void example_unwrap() {
    auto num = foo().unwrap(); // 42
    auto err = bar().unwrap_err(); // "Not implemented"
    auto bad_variant_access = bar().unwrap(); // This line will throw std::bad_variant_access
  }
  ```
* Result chaining:
  ```c++
  void example_result_chaining() {
    Result<std::string, int> res = foo()
      .then([](int num) {
        return std::to_string(num); // converts the ok type to std::string
      }).except([](std::string) {
        return -1;                  // converts the error type to int
      });
  }
  ```
* Throwing error using `TRY` macro (you may need to disable the `-pedantic` flag):
  ```c++
  #include <delameta/error.h> // for TRY macro

  std::string try_macro_check;

  auto process_foo() -> Result<int, std::string> {
    int num = TRY(foo());
    try_macro_check = "The code will be here";

    num = TRY(bar()); // Since bar() returns an Err variant, the error will be returned
    try_macro_check = "The code won't be here";
    return Ok(num);
  }

  void example_try_macro() {
    auto res = process_foo();
    assert(try_macro_check == "The code will be here");
    assert(res.is_err());
  }
  ```

### Json Serialization/Deserialization
* Example adding json traits for an existing struct:
  ```c++
  #include <delameta/json.h>

  struct Foo {
    int num;
    bool is_true;
  };

  namespace Project::etl::json {
    template<> size_t size_max(const Foo& m) {
      //     {   "num"             :   <int>             ,   "is_true"             :   <bool>                }
      return 1 + size_max("num") + 1 + size_max(m.num) + 1 + size_max("is_true") + 1 + size_max(m.is_true) + 1;
    }
    
    template<> std::string serialize(const Foo& m) {
      std::string res;
      res.reserve(size_max(m));
      res += '{';
      res += "\"num\":";
      res += serialize(m.num);
      res += ",\"is_true\":";
      res += serialize(m.is_true);
      res += '}';
      return res;
    }
    
    template<> constexpr // yes, you can deserialize json in constexpr context
    etl::Result<void, const char*> deserialize(const etl::Json& j, Foo& m) {
      if (!j.is_dictionary()) return etl::Err("JSON is not a map");
      
      auto num = detail::json_deserialize(j, "num", m.num);
      if (num.is_err()) return num;

      auto is_true = detail::json_deserialize(j, "is_true", m.is_true);
      if (is_true.is_err()) return is_true;

      return etl::Ok();
    }
  }

  void json_check() {
    constexpr Foo foo = {
      .num = 42,
      .is_true = false,
    };

    constexpr char str_foo[] = R"({"num":42,"is_true":false})";

    using namespace Project::etl;
    std::string ser_foo = json::serialize(foo);
    assert(ser_foo == str_foo);

    constexpr Foo foo_expect = json::deserialize<Foo>(str_foo).unwrap();
    assert(foo_expect.num == foo.num);
    assert(foo_expect.is_true == foo.is_true);
  }
  ```
* Or you can use `JSON_DECLARE` macro to automatically declare struct and provide the json traits (needs boost preprocessor):
  ```c++
  #include <boost/preprocessor.hpp> // Ensure boost preprocessor is included before anything else
  #include <delameta/json.h>

  JSON_DECLARE(
    (Foo)
    ,
    (int  , num    )
    (bool , is_true)
  )
  ```

### Stream
Stream is basically a list of `Stream::Rule` which is an `std::function<std::string_view(Stream&)>`. 
You can add a rule using `operator<<`.
```c++
#include <boost/preprocessor.hpp>
#include <delameta/stream.h>
#include <fstream>

using namespace Project;
using delameta::Stream;
using delameta::Result;
using delameta::Error;
using etl::Err;
using etl::Ok;

auto example_stream() -> Result<Stream> {
  auto file = new std::ifstream("some_long_text.txt");
  if (!file->is_open()) {
    delete file;
    return Err(Error{-1, "Error: Could not open the file!"});
  }

  Stream s;
  s << [file, buffer=std::string()](Stream& s) mutable -> std::string_view {
    // indicates that this rule may be used again
    s.again = bool(std::getline(*file, buffer));
    return s.again ? buffer : "";
  };

  s.at_destructor = [file]() {
    file->close();
    delete file;
  };

  return Ok(std::move(s));
}
```

You can output the data using `operator>>` which takes an `std::function<void(std::string_view)>`
as the argument.
```c++
auto example_stream_out() -> Result<void> {
  Stream s = TRY(example_stream());

  // print out each line
  s >> [](std::string_view sv) {
    std::cout << sv;
  };

  return Ok();
}
```

### And more
Feel free to head over to [app](app/) and [test](test/) for some more examples
