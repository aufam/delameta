#ifndef PROJECT_DELAMETA_MODBUS_API_H
#define PROJECT_DELAMETA_MODBUS_API_H

#include "delameta/error.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace Project::delameta::modbus {
    enum FunctionCode {
        FunctionCodeReadCoils = 1,
        FunctionCodeReadDiscreteInputs = 2,
        FunctionCodeReadHoldingRegisters = 3,
        FunctionCodeReadInputRegisters = 4,
        FunctionCodeWriteSingleCoil = 5,
        FunctionCodeWriteSingleRegister = 6,
        FunctionCodeReadExceptionStatus = 7,
        FunctionCodeDiagnostic = 8,
        FunctionCodeWriteMultipleCoils = 15,
        FunctionCodeWriteMultipleRegisters = 16,
    };

    bool is_valid(const std::vector<uint8_t>& data);
    std::vector<uint8_t>& add_checksum(std::vector<uint8_t>& data);

    class Error : public delameta::Error {
    public:
        enum Code {
            InvalidCRC,
            InvalidAddress,
            UnknownRegister,
            UnknownFunctionCode,
            UnknownSubfunction,
            InvalidDataFrame,
            InvalidSetValue,
            ExceptionStatusIsNotDefined,
        };

        using delameta::Error::Error;

        Error(Code code);
        Error(delameta::Error&& err);
        virtual ~Error() = default;
    };

    template <typename T>
    using Result = etl::Result<T, Error>;
}

#endif