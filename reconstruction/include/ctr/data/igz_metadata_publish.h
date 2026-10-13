#pragma once
#include "ctr/data/igz_metadata_append.h"
namespace ctr::data {
// Whole01580940. receiver+50 destination list; RSI source list.
// Null destination requires the original manager/context/factory, still missing.
void igz_metadata_publish(IgzMetadataList* destination,
                         std::int32_t source_count, const std::byte* source);
}
