#ifndef __ILRD_LOCALSTORAGE_HPP
#define __ILRD_LOCALSTORAGE_HPP

#include <cstddef> //size_t
#include <memory> //std::shared_ptr
#include <string> //std::string
#include <vector> //std::vector

#include "DriverData.hpp" //DriverData
#include "IStorage.hpp" //IStorage

namespace hrd41
{
    class LocalStorage: public IStorage
    {
    
    public:
        explicit LocalStorage(size_t size);
        LocalStorage(const LocalStorage& other_) = delete;
        LocalStorage& operator=(const LocalStorage& other_) = delete;
        ~LocalStorage() override = default;

        void Read(std::shared_ptr<DriverData> data_) override;
        void Write(std::shared_ptr<DriverData> data_) override;
    private:
        std::vector<char> m_storage;
    };

    class LocalStorageError: public StorageError
    {
        public:
            explicit LocalStorageError(const std::string& msg_);
            ~LocalStorageError() override = default;  
    };
} //namespace hrd41

#endif //__ILRD_LOCAL_STORAGE_HPP