#ifndef __ILRD_DRIVER_DATA_HPP__
#define __ILRD_DRIVER_DATA_HPP__

#include <cstddef>
#include <vector>

namespace hrd41
{

struct DriverData
{
    enum ActionType
    {
        READ,
        WRITE,
        DISCONNECT,
        FLUSH,
        TRIM,
        WRITE_ZEROES
    };

    enum StatusType
    {
        SUCCESS,
        FAILURE
    };

    explicit DriverData(ActionType type_, size_t handle_, size_t offset_,  size_t nBytes_, StatusType status_ = SUCCESS);
    DriverData(const DriverData& other_) = default;
    DriverData& operator=(const DriverData& other_) = default;
    ~DriverData() = default;

    ActionType m_type;
    size_t m_handle;
    size_t m_offset;
    StatusType m_status;
    std::vector<char> m_buffer;
};

} // namespace hrd41

#endif // __ILRD_DRIVER_DATA_HPP__