#include "ctr/gameplay/vector_record.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void update_vector_record(std::span<std::uint8_t> records, std::int32_t count,
                          std::int32_t key, std::span<const std::uint8_t, 12> vector) {
    if (count < 0 || static_cast<std::size_t>(count) > records.size() / 32)
        throw std::invalid_argument("Missing valid vector record extent");
    for (std::int32_t i = 0; i < count; ++i) {
        auto* record = records.data() + static_cast<std::size_t>(i) * 32;
        std::int32_t candidate;
        std::memcpy(&candidate, record, 4);
        if (candidate == key) {
            std::memmove(record + 16, vector.data(), 12);
            std::memset(record + 28, 0, 4);
            return;
        }
    }
}
}
