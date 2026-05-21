#ifndef __ILRD_ISTORAGE_HPP__
#define __ILRD_ISTORAGE_HPP__

#include <cstddef> //size_t
#include <memory> //std::shared_ptr
#include <stdexcept> //std::runtime_error
#include <string> //std::string

#include "DriverData.hpp" //DriverData

namespace hrd41
{

    class IStorage
    {

    public:
        explicit IStorage(size_t size);
        IStorage(const IStorage& other_) = delete;
        IStorage& operator=(const IStorage& other_) = delete;
        virtual ~IStorage() = default;

        virtual void Read(std::shared_ptr<DriverData> data_) = 0;
        virtual void Write(std::shared_ptr<DriverData> data_) = 0;

        size_t GetSize() const;

    private:
        size_t m_size;
    };

    class StorageError: public std::runtime_error
    {
        public:
            explicit StorageError(const std::string& msg_);
            ~StorageError() override;
    };


} //namespace hrd41

#endif //__ILRD_ISTORAGE_HPP__
