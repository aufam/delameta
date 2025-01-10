## [0.2.3] - 2025-01-10
### Added
- Http::listen

### Fixed
- File and socket descriptor implementation on windows

## [0.2.2] - 2025-01-03
### Fixed
- Duplicate client socket on aarch

## [0.2.1] - 2024-12-31
### Improved
- Using system default ca certificates for HTTPS request
- Better system error handling

### Fixed
- UDP server endpoint

## [0.2.0] - 2024-12-03
### Changed
- Debug macros

### Improved
- Some improvements

### Added
- Support for MinGW

## [0.1.10] - 2024-12-03
### Fixed
- TLS read implementation
- URL debug formatter

## [0.1.9] - 2024-12-02
### Fixed
- Adding small delay after TCP/UDP write in STM32 TCP/UDP implementation
- Http response header for chunked transfer encoding
- Http chunked decode implementation

### Changed
- HTTP header is streamed per item

## [0.1.8] - 2024-11-28
### Added
- Option to skip install target

### Fixed
- Some warnings

### Removed
- jwt-cpp from main app

## [0.1.7] - 2024-11-13
### Added
- Use unordered_multimap as http routers

## [0.1.6] - 2024-10-22
### Added
- Improve HTTP routing style
- HTTP handling for static directory

## [0.1.5] - 2024-10-15
### Added
- Optional print response header in main opts
- TLS certificate and key global variable for https request
- TLS and TCP server to use event-driven pattern
- StringStream

### Fixed
- Handling incomplete HTTP headers
- Improve chunked decoding

## [0.1.4] - 2024-10-13
### Added
- A01 distance sensor
- Optional 16 bits size of modbus response length of read register (for SHZK and FS50L)
- Optional timeout in TCP server

### Fixed
- Changlog sorted newest first
- Break TCP server when timeout

## [0.1.3] - 2024-10-04
### Added
- Multistage docker build

### Fixed
- Handle timeout == 0

## [0.1.2] - 2024-10-03
### Added
- Docker script

### Fixed
- Static link for app build

## [0.1.1] - 2024-09-25
### Added
- Handling for transfer encoding chunked

### Fixed
- Static linking for libssl

