#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/nbd.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "NBDDriverComm.hpp"

#ifndef NBD_CMD_MASK_COMMAND
#define NBD_CMD_MASK_COMMAND 0x0000ffffU
#endif

namespace
{

enum NBDCommandNumbers
{
    CMD_READ = 0,
    CMD_WRITE = 1,
    CMD_DISC = 2,
    CMD_FLUSH = 3,
    CMD_TRIM = 4,
    CMD_WRITE_ZEROES = 6
};

uint64_t ntohll(uint64_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return value;
#else
    uint32_t lo = static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    uint32_t hi = static_cast<uint32_t>(value >> 32U);

    lo = ntohl(lo);
    hi = ntohl(hi);

    return (static_cast<uint64_t>(lo) << 32U) | hi;
#endif
}

void CloseFd(int& fd_)
{
    if (-1 != fd_)
    {
        close(fd_);
        fd_ = -1;
    }
}

void ReadAll(int fd_, void *buffer_, size_t count_)
{
    char *runner = static_cast<char *>(buffer_);
    size_t total = 0;

    while (total < count_)
    {
        const ssize_t bytesRead = read(fd_, runner + total, count_ - total);

        if (-1 == bytesRead)
        {
            if (EINTR == errno)
            {
                continue;
            }

            std::perror("ReadAll");
            throw std::runtime_error("ReadAll failed");
        }

        if (0 == bytesRead)
        {
            throw std::runtime_error("ReadAll got EOF");
        }

        total += static_cast<size_t>(bytesRead);
    }
}

void WriteAll(int fd_, const void *buffer_, size_t count_)
{
    const char *runner = static_cast<const char *>(buffer_);
    size_t total = 0;

    while (total < count_)
    {
        const ssize_t bytesWritten = write(fd_, runner + total, count_ - total);

        if (-1 == bytesWritten)
        {
            if (EINTR == errno)
            {
                continue;
            }

            std::perror("WriteAll");
            throw std::runtime_error("WriteAll failed");
        }

        if (0 == bytesWritten)
        {
            throw std::runtime_error("WriteAll wrote 0 bytes");
        }

        total += static_cast<size_t>(bytesWritten);
    }
}

void CleanupInitFailure(int& nbdFd_, int& serverFd_, int& clientFd_)
{
    CloseFd(nbdFd_);
    CloseFd(serverFd_);
    CloseFd(clientFd_);
}

void InitCommonImpl(const std::string& deviceName_,
                    int& nbdFd_,
                    int& serverFd_,
                    int& clientFd_)
{
    int sockets[2] = {-1, -1};

    if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, sockets))
    {
        throw hrd41::NBDDriverError("socketpair failed");
    }

    serverFd_ = sockets[0];
    clientFd_ = sockets[1];

    nbdFd_ = open(deviceName_.c_str(), O_RDWR);
    if (-1 == nbdFd_)
    {
        CleanupInitFailure(nbdFd_, serverFd_, clientFd_);
        throw hrd41::NBDDriverError("open nbd device failed");
    }

    if (-1 == ioctl(nbdFd_, NBD_CLEAR_SOCK))
    {
        if (EINVAL != errno && ENOTTY != errno)
        {
            CleanupInitFailure(nbdFd_, serverFd_, clientFd_);
            throw hrd41::NBDDriverError("NBD_CLEAR_SOCK failed");
        }
    }

#ifdef NBD_FLAG_SEND_FLUSH
    if (-1 == ioctl(nbdFd_, NBD_SET_FLAGS, NBD_FLAG_SEND_FLUSH))
    {
        std::perror("NBD_SET_FLAGS");
    }
#endif
}

void ListenerRoutineImpl(int nbdFd_)
{
    if (-1 == ioctl(nbdFd_, NBD_DO_IT))
    {
        if (EPIPE != errno && ECONNRESET != errno && ENOTCONN != errno)
        {
            std::perror("NBD_DO_IT");
        }
    }

#ifdef NBD_CLEAR_QUE
    if (-1 == ioctl(nbdFd_, NBD_CLEAR_QUE))
    {
        if (EINVAL != errno && ENOTTY != errno)
        {
            std::perror("NBD_CLEAR_QUE");
        }
    }
#endif

    if (-1 == ioctl(nbdFd_, NBD_CLEAR_SOCK))
    {
        if (EINVAL != errno && ENOTTY != errno)
        {
            std::perror("NBD_CLEAR_SOCK");
        }
    }
}

} // anonymous namespace

namespace hrd41
{

NBDDriverComm::NBDDriverComm(const std::string& deviceName_,
                             size_t storage_size)
    : m_nbdFd(-1)
    , m_serverFd(-1)
    , m_clientFd(-1)
    , m_listener()
{
    InitCommonImpl(deviceName_, m_nbdFd, m_serverFd, m_clientFd);

    if (-1 == ioctl(m_nbdFd, NBD_SET_BLKSIZE, 4096))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_BLKSIZE failed");
    }

    if (-1 == ioctl(m_nbdFd, NBD_SET_SIZE, storage_size))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_SIZE failed");
    }

    if (-1 == ioctl(m_nbdFd, NBD_SET_SOCK, m_clientFd))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_SOCK failed");
    }

    m_listener = std::thread(ListenerRoutineImpl, m_nbdFd);
}

NBDDriverComm::NBDDriverComm(const std::string& deviceName_,
                             size_t block_size,
                             size_t num_blocks)
    : m_nbdFd(-1)
    , m_serverFd(-1)
    , m_clientFd(-1)
    , m_listener()
{
    InitCommonImpl(deviceName_, m_nbdFd, m_serverFd, m_clientFd);

    if (-1 == ioctl(m_nbdFd, NBD_SET_BLKSIZE, block_size))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_BLKSIZE failed");
    }

    if (-1 == ioctl(m_nbdFd, NBD_SET_SIZE_BLOCKS, num_blocks))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_SIZE_BLOCKS failed");
    }

    if (-1 == ioctl(m_nbdFd, NBD_SET_SOCK, m_clientFd))
    {
        CleanupInitFailure(m_nbdFd, m_serverFd, m_clientFd);
        throw NBDDriverError("NBD_SET_SOCK failed");
    }

    m_listener = std::thread(ListenerRoutineImpl, m_nbdFd);
}

NBDDriverComm::~NBDDriverComm()
{
    try
    {
        Disconnect();
    }
    catch (...)
    {
    }

    if (-1 != m_serverFd)
    {
        shutdown(m_serverFd, SHUT_RDWR);
    }

    if (-1 != m_clientFd)
    {
        shutdown(m_clientFd, SHUT_RDWR);
    }

    if (m_listener.joinable())
    {
        m_listener.join();
    }

    CloseFd(m_serverFd);
    CloseFd(m_clientFd);
    CloseFd(m_nbdFd);
}

std::shared_ptr<DriverData> NBDDriverComm::ReceiveRequest()
{
    struct nbd_request request;
    std::memset(&request, 0, sizeof(request));

    ReadAll(m_serverFd, &request, sizeof(request));

    if (request.magic != htonl(NBD_REQUEST_MAGIC))
    {
        throw NBDDriverError("invalid request magic");
    }

    const uint32_t rawType = ntohl(request.type);
    const uint32_t command = rawType & NBD_CMD_MASK_COMMAND;

    DriverData::ActionType action = DriverData::DISCONNECT;

    switch (command)
    {
        case CMD_READ:
            action = DriverData::READ;
            break;

        case CMD_WRITE:
            action = DriverData::WRITE;
            break;

        case CMD_FLUSH:
            action = DriverData::FLUSH;
            break;

        case CMD_TRIM:
            action = DriverData::TRIM;
            break;

        case CMD_WRITE_ZEROES:
            action = DriverData::WRITE_ZEROES;
            break;

        case CMD_DISC:
            action = DriverData::DISCONNECT;
            break;

        default:
            std::cerr << "unsupported NBD command: " << command << std::endl;
            throw NBDDriverError("unsupported NBD command");
    }

    uint64_t handle = 0;
    std::memcpy(&handle, request.handle, sizeof(handle));

    const size_t offset = static_cast<size_t>(ntohll(request.from));
    const size_t nBytes = static_cast<size_t>(ntohl(request.len));

    std::shared_ptr<DriverData> data(new DriverData(action, handle, offset, nBytes));

    if (DriverData::WRITE == action && 0 != nBytes)
    {
        ReadAll(m_serverFd, &data->m_buffer[0], nBytes);
    }

    return data;
}

void NBDDriverComm::SendReplay(std::shared_ptr<DriverData> data_)
{
    if (!data_)
    {
        throw NBDDriverError("SendReplay received null data");
    }

    if (DriverData::DISCONNECT == data_->m_type)
    {
        return;
    }

    struct nbd_reply reply;
    std::memset(&reply, 0, sizeof(reply));

    reply.magic = htonl(NBD_REPLY_MAGIC);
    reply.error = htonl((DriverData::SUCCESS == data_->m_status) ? 0 : EIO);

    std::memcpy(reply.handle, &data_->m_handle, sizeof(reply.handle));

    WriteAll(m_serverFd, &reply, sizeof(reply));

    if (DriverData::READ == data_->m_type &&
        DriverData::SUCCESS == data_->m_status &&
        !data_->m_buffer.empty())
    {
        WriteAll(m_serverFd, &data_->m_buffer[0], data_->m_buffer.size());
    }
}

void NBDDriverComm::Disconnect()
{
    if (-1 == m_nbdFd)
    {
        return;
    }

    if (-1 == ioctl(m_nbdFd, NBD_DISCONNECT))
    {
        if (EINVAL != errno && ENOTTY != errno && ENOTCONN != errno)
        {
            throw NBDDriverError("NBD_DISCONNECT failed");
        }
    }
}

int NBDDriverComm::GetFD()
{
    return m_serverFd;
}

NBDDriverError::NBDDriverError(const std::string& msg_)
    : DriverError(msg_)
{
}

} // namespace hrd41