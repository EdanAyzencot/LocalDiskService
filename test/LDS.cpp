#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <termios.h>
#include <unistd.h>

#include "IStorage.hpp"
#include "IDriverComm.hpp"
#include "LocalStorage.hpp"
#include "NBDDriverComm.hpp"

namespace
{

class FdGuard
{
public:
    explicit FdGuard(int fd_ = -1) : m_fd(fd_) {}
    ~FdGuard()
    {
        if (-1 != m_fd)
        {
            close(m_fd);
        }
    }

    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

    int Get() const
    {
        return m_fd;
    }

private:
    int m_fd;
};

class StdinRawModeGuard
{
public:
    StdinRawModeGuard() : m_enabled(false), m_oldFlags(0)
    {
        std::memset(&m_oldTermios, 0, sizeof(m_oldTermios));
    }

    void Enable()
    {
        if (m_enabled || 0 == isatty(STDIN_FILENO))
        {
            return;
        }

        if (-1 == tcgetattr(STDIN_FILENO, &m_oldTermios))
        {
            throw std::runtime_error("tcgetattr failed");
        }

        termios raw = m_oldTermios;
        raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (-1 == tcsetattr(STDIN_FILENO, TCSANOW, &raw))
        {
            throw std::runtime_error("tcsetattr failed");
        }

        m_oldFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (-1 == m_oldFlags)
        {
            tcsetattr(STDIN_FILENO, TCSANOW, &m_oldTermios);
            throw std::runtime_error("fcntl(F_GETFL) failed");
        }

        if (-1 == fcntl(STDIN_FILENO, F_SETFL, m_oldFlags | O_NONBLOCK))
        {
            tcsetattr(STDIN_FILENO, TCSANOW, &m_oldTermios);
            throw std::runtime_error("fcntl(F_SETFL) failed");
        }

        m_enabled = true;
    }

    ~StdinRawModeGuard()
    {
        if (m_enabled)
        {
            tcsetattr(STDIN_FILENO, TCSANOW, &m_oldTermios);
            fcntl(STDIN_FILENO, F_SETFL, m_oldFlags);
        }
    }

private:
    bool m_enabled;
    int m_oldFlags;
    termios m_oldTermios;
};

void PrintUsage(const char *prog_)
{
    std::cerr << "Usage:\n";
    std::cerr << prog_ << " <device> <storage_size_bytes>\n";
    std::cerr << "or\n";
    std::cerr << prog_ << " <device> <block_size> <num_blocks>\n";
}

void AddFdToEpollOrThrow(int epollFd_, int fd_)
{
    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = fd_;

    if (-1 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd_, &event))
    {
        throw std::runtime_error("epoll_ctl add failed");
    }
}

bool TryAddStdinToEpoll(int epollFd_)
{
    if (0 == isatty(STDIN_FILENO))
    {
        return false;
    }

    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;

    if (-1 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, STDIN_FILENO, &event))
    {
        return false;
    }

    return true;
}

int CreateSignalFd()
{
    sigset_t mask;

    if (-1 == sigemptyset(&mask) ||
        -1 == sigaddset(&mask, SIGINT) ||
        -1 == sigaddset(&mask, SIGTERM))
    {
        throw std::runtime_error("signal mask setup failed");
    }

    if (-1 == pthread_sigmask(SIG_BLOCK, &mask, 0))
    {
        throw std::runtime_error("pthread_sigmask failed");
    }

    const int signalFd = signalfd(-1, &mask, 0);
    if (-1 == signalFd)
    {
        throw std::runtime_error("signalfd failed");
    }

    return signalFd;
}

bool HandleStdinEvent()
{
    char ch = '\0';
    const ssize_t bytesRead = read(STDIN_FILENO, &ch, 1);

    if (bytesRead <= 0)
    {
        return false;
    }

    return ('q' == ch || 'Q' == ch);
}

bool HandleSignalEvent(int signalFd_)
{
    signalfd_siginfo fdsi;
    std::memset(&fdsi, 0, sizeof(fdsi));

    const ssize_t bytesRead = read(signalFd_, &fdsi, sizeof(fdsi));
    if (bytesRead != static_cast<ssize_t>(sizeof(fdsi)))
    {
        throw std::runtime_error("failed reading signalfd");
    }

    return (SIGINT == static_cast<int>(fdsi.ssi_signo) ||
            SIGTERM == static_cast<int>(fdsi.ssi_signo));
}

void HandleStorageRequest(const std::unique_ptr<hrd41::IStorage>& storage_,
                          const std::unique_ptr<hrd41::IDriverComm>& driver_,
                          std::shared_ptr<hrd41::DriverData> request_)
{
    try
    {
        switch (request_->m_type)
        {
            case hrd41::DriverData::READ:
                storage_->Read(request_);
                request_->m_status = hrd41::DriverData::SUCCESS;
                driver_->SendReplay(request_);
                break;

            case hrd41::DriverData::WRITE:
                storage_->Write(request_);
                request_->m_status = hrd41::DriverData::SUCCESS;
                driver_->SendReplay(request_);
                break;

            case hrd41::DriverData::FLUSH:
            case hrd41::DriverData::TRIM:
                request_->m_status = hrd41::DriverData::SUCCESS;
                driver_->SendReplay(request_);
                break;

            case hrd41::DriverData::WRITE_ZEROES:
                std::fill(request_->m_buffer.begin(), request_->m_buffer.end(), 0);
                request_->m_type = hrd41::DriverData::WRITE;
                storage_->Write(request_);
                request_->m_type = hrd41::DriverData::WRITE_ZEROES;
                request_->m_status = hrd41::DriverData::SUCCESS;
                driver_->SendReplay(request_);
                break;

            case hrd41::DriverData::DISCONNECT:
                break;

            default:
                request_->m_status = hrd41::DriverData::FAILURE;
                driver_->SendReplay(request_);
                break;
        }
    }
    catch (...)
    {
        request_->m_status = hrd41::DriverData::FAILURE;
        driver_->SendReplay(request_);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    try
    {
        std::unique_ptr<hrd41::IStorage> storage;
        std::unique_ptr<hrd41::IDriverComm> driver;

        if (3 == argc)
        {
            const std::string deviceName = argv[1];
            const size_t storageSize =
                static_cast<size_t>(std::strtoull(argv[2], 0, 10));

            storage.reset(new hrd41::LocalStorage(storageSize));
            driver.reset(new hrd41::NBDDriverComm(deviceName, storageSize));
        }
        else if (4 == argc)
        {
            const std::string deviceName = argv[1];
            const size_t blockSize =
                static_cast<size_t>(std::strtoull(argv[2], 0, 10));
            const size_t numBlocks =
                static_cast<size_t>(std::strtoull(argv[3], 0, 10));
            const size_t totalSize = blockSize * numBlocks;

            storage.reset(new hrd41::LocalStorage(totalSize));
            driver.reset(new hrd41::NBDDriverComm(deviceName, blockSize, numBlocks));
        }
        else
        {
            PrintUsage(argv[0]);
            return 1;
        }

        std::cout << "LDS is running.\n";
        std::cout << "Press q/Q or send SIGINT/SIGTERM to disconnect.\n";

        FdGuard epollFd(epoll_create1(0));
        if (-1 == epollFd.Get())
        {
            throw std::runtime_error("epoll_create1 failed");
        }

        FdGuard signalFd(CreateSignalFd());

        AddFdToEpollOrThrow(epollFd.Get(), driver->GetFD());
        AddFdToEpollOrThrow(epollFd.Get(), signalFd.Get());

        StdinRawModeGuard stdinGuard;
        bool stdinRegistered = false;

        if (isatty(STDIN_FILENO))
        {
            stdinGuard.Enable();
            stdinRegistered = TryAddStdinToEpoll(epollFd.Get());
        }

        bool shouldRun = true;
        epoll_event events[8];

        while (shouldRun)
        {
            const int ready = epoll_wait(epollFd.Get(), events, 8, -1);
            if (-1 == ready)
            {
                if (EINTR == errno)
                {
                    continue;
                }

                throw std::runtime_error("epoll_wait failed");
            }

            for (int i = 0; i < ready; ++i)
            {
                const int currentFd = events[i].data.fd;

                if (currentFd == driver->GetFD())
                {
                    std::shared_ptr<hrd41::DriverData> request = driver->ReceiveRequest();

                    if (hrd41::DriverData::DISCONNECT == request->m_type)
                    {
                        shouldRun = false;
                        break;
                    }

                    HandleStorageRequest(storage, driver, request);
                }
                else if (currentFd == signalFd.Get())
                {
                    if (HandleSignalEvent(signalFd.Get()))
                    {
                        driver->Disconnect();
                        shouldRun = false;
                        break;
                    }
                }
                else if (stdinRegistered && currentFd == STDIN_FILENO)
                {
                    if (HandleStdinEvent())
                    {
                        driver->Disconnect();
                        shouldRun = false;
                        break;
                    }
                }
            }
        }

        std::cout << "LDS stopped cleanly.\n";
        return 0;
    }
    
    catch (const std::exception& e)
    {
        std::cerr << "LDS error: " << e.what() << std::endl;
        return 1;
    }
}