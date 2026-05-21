#ifndef __ILRD_NBDDRIVERCOMM_HPP
#define __ILRD_NBDDRIVERCOMM_HPP

#include <cstddef> //size_t
#include <string> //std::string
#include <thread> //std::thread

#include "IDriverComm.hpp" //IDriverComm
#include "DriverData.hpp" //DriverData

namespace hrd41
{

class NBDDriverComm : public IDriverComm
{
public:
    explicit NBDDriverComm(const std::string& deviceName_, size_t storage_size);
    explicit NBDDriverComm(const std::string& deviceName_, size_t block_size, size_t num_blocks);
    NBDDriverComm(const NBDDriverComm& other_) = delete;
    NBDDriverComm& operator=(const NBDDriverComm& other_) = delete;
    ~NBDDriverComm() override;

    std::shared_ptr<DriverData> ReceiveRequest() override;
    void SendReplay(std::shared_ptr<DriverData> data_) override;
    void Disconnect() override;
    int GetFD() override;

private:
    int m_nbdFd;
    int m_serverFd;
    int m_clientFd;
    std::thread m_listener;
};

class NBDDriverError : public DriverError
{
public:
    explicit NBDDriverError(const std::string& msg_);
    ~NBDDriverError() override = default;
};

} //namespace hrd41

#endif //__ILRD_NBDDRIVERCOMM_HPP__