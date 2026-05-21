#include "LocalStorage.hpp"
#include <algorithm>

namespace hrd41
{

LocalStorage::LocalStorage(size_t size_)
    : IStorage(size_)
    , m_storage(size_, 0)
{
}

void LocalStorage::Read(std::shared_ptr<DriverData> data_)
{
    if (!data_)
    {
        throw LocalStorageError("LocalStorage::Read - null DriverData");
    }

    if (DriverData::READ != data_->m_type)
    {
        throw LocalStorageError("LocalStorage::Read - invalid action type");
    }

    if (data_->m_offset + data_->m_buffer.size() > m_storage.size())
    {
        throw LocalStorageError("LocalStorage::Read - out of bounds");
    }

    std::copy(m_storage.begin() + data_->m_offset,
              m_storage.begin() + data_->m_offset + data_->m_buffer.size(),
              data_->m_buffer.begin());

    data_->m_status = DriverData::SUCCESS;
}

void LocalStorage::Write(std::shared_ptr<DriverData> data_)
{
    if (!data_)
    {
        throw LocalStorageError("LocalStorage::Write - null DriverData");
    }

    if (DriverData::WRITE != data_->m_type)
    {
        throw LocalStorageError("LocalStorage::Write - invalid action type");
    }

    if (data_->m_offset + data_->m_buffer.size() > m_storage.size())
    {
        throw LocalStorageError("LocalStorage::Write - out of bounds");
    }

    std::copy(data_->m_buffer.begin(),
              data_->m_buffer.end(),
              m_storage.begin() + data_->m_offset);

    data_->m_status = DriverData::SUCCESS;
}

LocalStorageError::LocalStorageError(const std::string& msg_)
    : StorageError(msg_)
{
}

} // namespace hrd41