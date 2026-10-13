#pragma once
#include <cstdint>
namespace ctr {
// Whole 00ee6e30: ticks at +34/+38; active flag +30; data delay +1c.
float boost_age(bool active, std::int32_t now, std::int32_t started,
                std::int32_t finished, float delay);
}
