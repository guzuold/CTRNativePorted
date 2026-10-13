#include "ctr/data/igz_metadata_publish.h"
#include <stdexcept>
namespace ctr::data {
void igz_metadata_publish(IgzMetadataList* destination,
                         std::int32_t source_count, const std::byte* source) {
    if (!destination)
        throw std::runtime_error("missing original metadata list manager/context014ecc10/factory014db850");
    if (source_count == 0) return;
    igz_metadata_append(*destination, source_count, source);
}
}
