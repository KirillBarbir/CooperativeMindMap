#include "mind_map/utils/id/uuid.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace mind_map {
    std::string make_uuid_string() {
        static thread_local boost::uuids::random_generator gen;
        return boost::uuids::to_string(gen());
    }
}
