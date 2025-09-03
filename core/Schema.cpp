#include "Schema.h"

namespace ma {

size_t Schema::nullBitmapBytes() const {
    size_t bits = fields.size();
    return (bits + 7) / 8;
}

size_t Schema::maxSerializedSize() const {
    size_t sz = nullBitmapBytes();
    for (const auto& f : fields) {
        switch (f.type) {
        case FieldType::Int32:     sz += 4; break;
        case FieldType::Double:    sz += 8; break;
        case FieldType::Bool:      sz += 1; break;
        case FieldType::CharN:     sz += f.size; break;
        case FieldType::String:    sz += 2 + 65535; break;
        case FieldType::Date:      sz += 4; break;
        case FieldType::Currency:  sz += 8; break;
        }
    }
    return sz;
}

}
