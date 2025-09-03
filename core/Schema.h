#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace ma {

enum class FieldType : uint8_t {
    Int32   = 1,
    Double  = 2,
    Bool    = 3,
    CharN   = 4,
    String  = 5,
    Date    = 6,
    Currency= 7
};

struct Field {
    std::string name;
    FieldType type;
    uint16_t size;
};

struct Schema {
    std::string tableName;
    std::vector<Field> fields;

    size_t nullBitmapBytes() const;
    size_t maxSerializedSize() const;
    const Field& field(size_t i) const { return fields.at(i); }
};

}
