#include "IStorage.hpp"

namespace hrd41
{

IStorage::IStorage(size_t size_)
    : m_size(size_)
{
}

size_t IStorage::GetSize() const
{
    return m_size;
}

StorageError::StorageError(const std::string& msg_)
    : std::runtime_error(msg_)
{
}

StorageError::~StorageError() = default;

} // namespace hrd41