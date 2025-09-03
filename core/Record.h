#pragma once
#include "Schema.h"
#include <variant>
#include <optional>
#include <cstring>

namespace ma {

using Value = std::variant<int32_t, double, bool, std::string, int64_t>;

struct Record {
    std::vector<std::optional<Value>> values;

    static Record withFieldCount(size_t n) { Record r; r.values.resize(n); return r; }
};

struct RID {
    uint32_t pageId{};
    uint16_t slotId{};
};

class Serializer {
public:
    static std::vector<uint8_t> serialize(const Schema& schema, const Record& rec);
    static Record deserialize(const Schema& schema, const uint8_t* data, size_t len);
};

}
