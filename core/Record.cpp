#include "Record.h"
#include <stdexcept>

namespace ma {

static inline void setBit(uint8_t* buf, size_t bitIndex) {
    buf[bitIndex / 8] |= (1u << (bitIndex % 8));
}
static inline bool getBit(const uint8_t* buf, size_t bitIndex) {
    return (buf[bitIndex / 8] >> (bitIndex % 8)) & 1u;
}

std::vector<uint8_t> Serializer::serialize(const Schema& schema, const Record& rec) {
    if (rec.values.size() != schema.fields.size())
        throw std::runtime_error("Record field count mismatch");
    std::vector<uint8_t> out;
    size_t n = schema.fields.size();
    size_t nb = schema.nullBitmapBytes();
    out.resize(nb, 0);

    for (size_t i = 0; i < n; ++i) {
        if (!rec.values[i].has_value()) {
            setBit(out.data(), i);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        const auto& fld = schema.fields[i];
        if (!rec.values[i].has_value()) continue;

        const Value& v = rec.values[i].value();
        switch (fld.type) {
        case FieldType::Int32: {
            if (!std::holds_alternative<int32_t>(v)) throw std::runtime_error("Type mismatch Int32");
            int32_t x = std::get<int32_t>(v);
            uint8_t b[4]; std::memcpy(b, &x, 4);
            out.insert(out.end(), b, b+4);
            break;
        }
        case FieldType::Double: {
            if (!std::holds_alternative<double>(v)) throw std::runtime_error("Type mismatch Double");
            double x = std::get<double>(v);
            uint8_t b[8]; std::memcpy(b, &x, 8);
            out.insert(out.end(), b, b+8);
            break;
        }
        case FieldType::Bool: {
            if (!std::holds_alternative<bool>(v)) throw std::runtime_error("Type mismatch Bool");
            bool x = std::get<bool>(v);
            out.push_back(x ? 1 : 0);
            break;
        }
        case FieldType::CharN: {
            if (!std::holds_alternative<std::string>(v)) throw std::runtime_error("Type mismatch CharN");
            const std::string& s = std::get<std::string>(v);
            if (s.size() > fld.size) throw std::runtime_error("CharN overflow");
            out.insert(out.end(), s.begin(), s.end());
            for (size_t k = s.size(); k < fld.size; ++k) out.push_back(0);
            break;
        }
        case FieldType::String: {
            if (!std::holds_alternative<std::string>(v)) throw std::runtime_error("Type mismatch String");
            const std::string& s = std::get<std::string>(v);
            if (s.size() > 65535) throw std::runtime_error("String too long");
            uint16_t L = static_cast<uint16_t>(s.size());
            uint8_t b[2]; std::memcpy(b, &L, 2);
            out.insert(out.end(), b, b+2);
            out.insert(out.end(), s.begin(), s.end());
            break;
        }
        case FieldType::Date: {
            if (!std::holds_alternative<int32_t>(v)) throw std::runtime_error("Type mismatch Date (int32 yyyymmdd)");
            int32_t x = std::get<int32_t>(v);
            uint8_t b[4]; std::memcpy(b, &x, 4);
            out.insert(out.end(), b, b+4);
            break;
        }
        case FieldType::Currency: {
            if (!std::holds_alternative<int64_t>(v)) throw std::runtime_error("Type mismatch Currency (int64 minor units)");
            int64_t x = std::get<int64_t>(v);
            uint8_t b[8]; std::memcpy(b, &x, 8);
            out.insert(out.end(), b, b+8);
            break;
        }
        }
    }
    return out;
}

Record Serializer::deserialize(const Schema& schema, const uint8_t* data, size_t len) {
    size_t n = schema.fields.size();
    size_t nb = schema.nullBitmapBytes();
    if (len < nb) throw std::runtime_error("Corrupt record (null-bitmap too short)");
    Record r = Record::withFieldCount(n);
    const uint8_t* p = data;
    const uint8_t* pend = data + len;

    for (size_t i = 0; i < n; ++i) {
        bool isNull = getBit(p, i);
        if (isNull) r.values[i] = std::nullopt;
    }
    p += nb;

    for (size_t i = 0; i < n; ++i) {
        if (getBit(data, i)) { // null
            r.values[i] = std::nullopt;
            continue;
        }
        const auto& fld = schema.fields[i];
        switch (fld.type) {
        case FieldType::Int32: {
            if (p + 4 > pend) throw std::runtime_error("Corrupt record (Int32)");
            int32_t x; std::memcpy(&x, p, 4); p += 4;
            r.values[i] = x;
            break;
        }
        case FieldType::Double: {
            if (p + 8 > pend) throw std::runtime_error("Corrupt record (Double)");
            double x; std::memcpy(&x, p, 8); p += 8;
            r.values[i] = x;
            break;
        }
        case FieldType::Bool: {
            if (p + 1 > pend) throw std::runtime_error("Corrupt record (Bool)");
            bool x = (*p != 0); p += 1;
            r.values[i] = x;
            break;
        }
        case FieldType::CharN: {
            if (p + fld.size > pend) throw std::runtime_error("Corrupt record (CharN)");
            std::string s(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p + fld.size));
            while (!s.empty() && s.back() == '\0') s.pop_back();
            p += fld.size;
            r.values[i] = s;
            break;
        }
        case FieldType::String: {
            if (p + 2 > pend) throw std::runtime_error("Corrupt record (String len)");
            uint16_t L; std::memcpy(&L, p, 2); p += 2;
            if (p + L > pend) throw std::runtime_error("Corrupt record (String data)");
            std::string s(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p + L)); p += L;
            r.values[i] = s;
            break;
        }
        case FieldType::Date: {
            if (p + 4 > pend) throw std::runtime_error("Corrupt record (Date)");
            int32_t x; std::memcpy(&x, p, 4); p += 4;
            r.values[i] = x;
            break;
        }
        case FieldType::Currency: {
            if (p + 8 > pend) throw std::runtime_error("Corrupt record (Currency)");
            int64_t x; std::memcpy(&x, p, 8); p += 8;
            r.values[i] = x;
            break;
        }
        }
    }
    return r;
}

}
