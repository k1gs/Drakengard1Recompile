#pragma once
#include <stdexcept>
namespace drakengard {
// Unsupported host device behavior is fatal, not a guest address exception/zero read.
class StrictDeviceError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
}
