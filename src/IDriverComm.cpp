#include "IDriverComm.hpp"

namespace hrd41
{

DriverError::DriverError(const std::string& msg_)
    : std::runtime_error(msg_)
{
}

DriverError::~DriverError() = default;

} // namespace hrd41