#ifndef __ILRD_IDRIVER_COMM_HPP__
#define __ILRD_IDRIVER_COMM_HPP__

#include <memory> // std::shared_ptr
#include <stdexcept> // std::runtime_error
#include <string> // std::string

#include "DriverData.hpp" //DriverData

namespace hrd41
{

class IDriverComm
{
public:
    IDriverComm() = default;
    IDriverComm(const IDriverComm& other_) = delete;
    IDriverComm& operator=(const IDriverComm& other_) = delete;
    virtual ~IDriverComm() = default;

    virtual std::shared_ptr<DriverData> ReceiveRequest() = 0;
    virtual void SendReplay(std::shared_ptr<DriverData> data_) = 0;
    virtual void Disconnect() = 0;
    virtual int GetFD() = 0;
};

class DriverError : public std::runtime_error
{
public:
    explicit DriverError(const std::string& msg_);
    ~DriverError() override;
};

} // namespace hrd41

#endif // __ILRD_IDRIVER_COMM_HPP__