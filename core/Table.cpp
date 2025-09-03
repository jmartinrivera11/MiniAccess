#include "Table.h"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace ma {

static constexpr uint32_t META_MAGIC = 0x4D455431u;

void Table::writeMeta() {
    std::ofstream out(metaPath_, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot write meta: " + metaPath_);
    out.write(reinterpret_cast<const char*>(&META_MAGIC), 4);
    uint16_t ver = 1; out.write(reinterpret_cast<const char*>(&ver), 2);
    uint16_t nameLen = static_cast<uint16_t>(schema_.tableName.size());
    out.write(reinterpret_cast<const char*>(&nameLen), 2);
    out.write(schema_.tableName.data(), nameLen);
    uint16_t n = static_cast<uint16_t>(schema_.fields.size());
    out.write(reinterpret_cast<const char*>(&n), 2);
    for (const auto& f : schema_.fields) {
        uint16_t flen = static_cast<uint16_t>(f.name.size());
        out.write(reinterpret_cast<const char*>(&flen), 2);
        out.write(f.name.data(), flen);
        uint8_t t = static_cast<uint8_t>(f.type);
        out.write(reinterpret_cast<const char*>(&t), 1);
        out.write(reinterpret_cast<const char*>(&f.size), 2);
    }
}

void Table::readMeta() {
    std::ifstream in(metaPath_, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open meta: " + metaPath_);
    uint32_t magic; in.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != META_MAGIC) throw std::runtime_error("Invalid meta magic");
    uint16_t ver; in.read(reinterpret_cast<char*>(&ver), 2);
    if (ver != 1) throw std::runtime_error("Meta version unsupported");
    uint16_t nameLen; in.read(reinterpret_cast<char*>(&nameLen), 2);
    schema_.tableName.resize(nameLen);
    in.read(schema_.tableName.data(), nameLen);
    uint16_t n; in.read(reinterpret_cast<char*>(&n), 2);
    schema_.fields.clear();
    schema_.fields.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        uint16_t flen; in.read(reinterpret_cast<char*>(&flen), 2);
        std::string fname(flen, '\0');
        in.read(fname.data(), flen);
        uint8_t t; in.read(reinterpret_cast<char*>(&t), 1);
        uint16_t sz; in.read(reinterpret_cast<char*>(&sz), 2);
        schema_.fields.push_back(Field{fname, static_cast<FieldType>(t), sz});
    }
}

void Table::create(const std::string& basePath, const Schema& schema) {
    basePath_ = basePath;
    metaPath_ = basePath_ + ".meta";
    madPath_  = basePath_ + ".mad";
    schema_ = schema;
    writeMeta();
    storage_.create(madPath_);
    avail_.clear();
}

void Table::open(const std::string& basePath) {
    basePath_ = basePath;
    metaPath_ = basePath_ + ".meta";
    madPath_  = basePath_ + ".mad";
    readMeta();
    storage_.open(madPath_);
    rebuildAvailFromPages();
}

void Table::close() {
    storage_.close();
    avail_.clear();
}

void Table::rebuildAvailFromPages() {
    avail_.clear();
    for (uint32_t pid = 1; pid < storage_.pageCount(); ++pid) {
        Page p = storage_.readPage(pid);
        for (uint16_t i = 0; i < p.hdr.slotCount; ++i) {
            Slot s = p.getSlot(i);
            if (slotIsFree(s) && slotLen(s) > 0) {
                avail_.add(FreeSlotRef{pid, i, slotLen(s)});
            }
        }
    }
}

RID Table::insert(const Record& rec) {
    auto payload = Serializer::serialize(schema_, rec);

    if (auto rid = tryInsertIntoFreeSlot(rec, payload)) {
        if (idxInt32_) {
            const auto& v = rec.values[idxInt32Field_];
            if (v.has_value()) {
                int32_t key = std::get<int32_t>(v.value());
                idxInt32_->insert(key, *rid);
            }
        }
        if (idxString_) {
            const auto& vs = rec.values[idxStringField_];
            if (vs.has_value()) {
                const std::string& s = std::get<std::string>(vs.value());
                idxString_->insert(s, *rid);
            }
        }
        return *rid;
    }

    if (auto rid = tryInsertIntoPages(payload)) {
        if (idxInt32_) {
            const auto& v = rec.values[idxInt32Field_];
            if (v.has_value()) {
                int32_t key = std::get<int32_t>(v.value());
                idxInt32_->insert(key, *rid);
            }
        }
        if (idxString_) {
            const auto& vs = rec.values[idxStringField_];
            if (vs.has_value()) {
                const std::string& s = std::get<std::string>(vs.value());
                idxString_->insert(s, *rid);
            }
        }
        return *rid;
    }

    uint32_t pid = storage_.allocatePage();
    Page p = storage_.readPage(pid);
    if (p.freeSpace() < payload.size()) throw std::runtime_error("Record larger than page capacity");

    uint16_t off = p.hdr.freeStart;
    std::memcpy(p.bytes.data() + off, payload.data(), payload.size());
    p.hdr.freeStart += static_cast<uint16_t>(payload.size());

    p.hdr.slotCount += 1;
    uint16_t slotIdx = p.hdr.slotCount - 1;
    Slot s{off, static_cast<uint16_t>(payload.size())};
    markSlotUsed(s);
    p.hdr.freeEnd -= sizeof(Slot);
    p.setSlot(slotIdx, s);
    storage_.writePage(p);

    RID rid{pid, slotIdx};

    if (idxInt32_) {
        const auto& v = rec.values[idxInt32Field_];
        if (v.has_value()) {
            int32_t key = std::get<int32_t>(v.value());
            idxInt32_->insert(key, rid);
        }
    }
    if (idxString_) {
        const auto& vs = rec.values[idxStringField_];
        if (vs.has_value()) {
            const std::string& sKey = std::get<std::string>(vs.value());
            idxString_->insert(sKey, rid);
        }
    }

    return rid;
}

std::optional<RID> Table::tryInsertIntoFreeSlot(const Record&, const std::vector<uint8_t>& payload) {
    uint16_t need = static_cast<uint16_t>(payload.size());
    auto chosen = avail_.acquire(need, fit_);
    if (!chosen) return std::nullopt;

    Page p = storage_.readPage(chosen->pageId);
    Slot s = p.getSlot(chosen->slotId);
    if (!slotIsFree(s) || slotLen(s) < need) {
        return std::nullopt;
    }
    std::memcpy(p.bytes.data() + s.offset, payload.data(), payload.size());
    s.length = static_cast<uint16_t>((s.length & 0x8000u) | need);
    markSlotUsed(s);
    p.setSlot(chosen->slotId, s);
    storage_.writePage(p);
    return RID{chosen->pageId, chosen->slotId};
}

std::optional<RID> Table::tryInsertIntoPages(const std::vector<uint8_t>& payload) {
    uint16_t need = static_cast<uint16_t>(payload.size());
    for (uint32_t pid = 1; pid < storage_.pageCount(); ++pid) {
        Page p = storage_.readPage(pid);
        if (p.freeSpace() >= need) {
            uint16_t off = p.hdr.freeStart;
            std::memcpy(p.bytes.data() + off, payload.data(), payload.size());
            p.hdr.freeStart += need;

            p.hdr.slotCount += 1;
            uint16_t slotIdx = p.hdr.slotCount - 1;
            Slot s{off, need};
            markSlotUsed(s);
            p.hdr.freeEnd -= sizeof(Slot);
            p.setSlot(slotIdx, s);
            storage_.writePage(p);
            return RID{pid, slotIdx};
        }
    }
    return std::nullopt;
}

std::optional<Record> Table::read(const RID& rid) {
    if (rid.pageId == 0) return std::nullopt;
    Page p = storage_.readPage(rid.pageId);
    if (rid.slotId >= p.hdr.slotCount) return std::nullopt;
    Slot s = p.getSlot(rid.slotId);
    if (slotIsFree(s) || slotLen(s)==0) return std::nullopt;
    const uint8_t* data = p.bytes.data() + s.offset;
    return Serializer::deserialize(schema_, data, slotLen(s));
}

bool Table::erase(const RID& rid) {
    if (rid.pageId == 0) return false;
    Page p = storage_.readPage(rid.pageId);
    if (rid.slotId >= p.hdr.slotCount) return false;
    Slot s = p.getSlot(rid.slotId);
    if (slotIsFree(s)) return false;

    std::optional<Record> rec = Serializer::deserialize(schema_, p.bytes.data() + s.offset, slotLen(s));

    int32_t keyForIndex = 0; bool hasKey = false;
    if (idxInt32_ && rec && rec->values[idxInt32Field_].has_value()) {
        keyForIndex = std::get<int32_t>(rec->values[idxInt32Field_].value());
        hasKey = true;
    }

    std::string strKeyForIndex; bool hasStrKey = false;
    if (idxString_ && rec && rec->values[idxStringField_].has_value()) {
        strKeyForIndex = std::get<std::string>(rec->values[idxStringField_].value());
        hasStrKey = true;
    }

    markSlotFree(s);
    p.setSlot(rid.slotId, s);
    storage_.writePage(p);
    avail_.add(FreeSlotRef{rid.pageId, rid.slotId, slotLen(s)});

    if (idxInt32_ && hasKey) {
        idxInt32_->erase(keyForIndex, rid);
    }
    if (idxString_ && hasStrKey) {
        idxString_->erase(strKeyForIndex, rid);
    }
    return true;
}

std::optional<RID> Table::update(const RID& rid, const Record& rec) {
    auto payload = Serializer::serialize(schema_, rec);
    if (rid.pageId == 0) return std::nullopt;
    Page p = storage_.readPage(rid.pageId);
    if (rid.slotId >= p.hdr.slotCount) return std::nullopt;
    Slot s = p.getSlot(rid.slotId);
    if (slotIsFree(s)) return std::nullopt;

    std::optional<Record> oldRec = Serializer::deserialize(schema_, p.bytes.data() + s.offset, slotLen(s));
    int32_t oldKey = 0; bool hasOld = false;
    if (idxInt32_ && oldRec && oldRec->values[idxInt32Field_].has_value()) {
        oldKey = std::get<int32_t>(oldRec->values[idxInt32Field_].value());
        hasOld = true;
    }
    std::string oldStr; bool hasOldStr = false;
    if (idxString_ && oldRec && oldRec->values[idxStringField_].has_value()) {
        oldStr = std::get<std::string>(oldRec->values[idxStringField_].value());
        hasOldStr = true;
    }

    uint16_t need = static_cast<uint16_t>(payload.size());
    uint16_t have = slotLen(s);

    if (need <= have) {
        std::memcpy(p.bytes.data() + s.offset, payload.data(), payload.size());
        s.length = (s.length & 0x8000u) | need;
        markSlotUsed(s);
        p.setSlot(rid.slotId, s);
        storage_.writePage(p);

        if (idxInt32_) {
            if (hasOld) idxInt32_->erase(oldKey, rid);
            const auto& vnew = rec.values[idxInt32Field_];
            if (vnew.has_value()) {
                int32_t newKey = std::get<int32_t>(vnew.value());
                idxInt32_->insert(newKey, rid);
            }
        }
        if (idxString_) {
            if (hasOldStr) idxString_->erase(oldStr, rid);
            const auto& vnewS = rec.values[idxStringField_];
            if (vnewS.has_value()) {
                const std::string& newStr = std::get<std::string>(vnewS.value());
                idxString_->insert(newStr, rid);
            }
        }
        return rid;
    } else {
        markSlotFree(s);
        p.setSlot(rid.slotId, s);
        storage_.writePage(p);
        avail_.add(FreeSlotRef{rid.pageId, rid.slotId, have});

        if (idxInt32_ && hasOld)    idxInt32_->erase(oldKey, rid);
        if (idxString_ && hasOldStr) idxString_->erase(oldStr, rid);

        return insert(rec);
    }
}

size_t Table::scanCount() {
    size_t cnt = 0;
    for (uint32_t pid = 1; pid < storage_.pageCount(); ++pid) {
        Page p = storage_.readPage(pid);
        for (uint16_t i = 0; i < p.hdr.slotCount; ++i) {
            Slot s = p.getSlot(i);
            if (!slotIsFree(s) && slotLen(s)>0) cnt++;
        }
    }
    return cnt;
}

std::vector<RID> Table::scanAll() {
    std::vector<RID> rids;
    for (uint32_t pid = 1; pid < storage_.pageCount(); ++pid) {
        Page p = storage_.readPage(pid);
        for (uint16_t i = 0; i < p.hdr.slotCount; ++i) {
            Slot s = p.getSlot(i);
            if (!slotIsFree(s) && slotLen(s)>0) rids.push_back(RID{pid, i});
        }
    }
    return rids;
}

bool Table::createInt32Index(int fieldIndex, const std::string& name) {
    if (fieldIndex < 0 || fieldIndex >= (int)schema_.fields.size()) return false;
    if (schema_.fields[fieldIndex].type != FieldType::Int32) return false;
    idxInt32_ = std::make_unique<IndexInt32>();
    idxInt32Field_ = fieldIndex;
    IndexInt32Desc d;
    d.name = name;
    d.fieldIndex = fieldIndex;
    d.path = basePath_ + "." + name + ".idx";
    idxInt32_->create(d);

    auto rids = scanAll();
    for (const auto& rid : rids) {
        auto rec = read(rid);
        if (!rec) continue;
        const auto& v = rec->values[fieldIndex];
        if (!v.has_value()) continue;
        int32_t key = std::get<int32_t>(v.value());
        idxInt32_->insert(key, rid);
    }
    return true;
}

std::vector<RID> Table::findByInt32(int fieldIndex, int32_t key) {
    if (!idxInt32_ || fieldIndex != idxInt32Field_) return {};
    return idxInt32_->find(key);
}

std::vector<RID> Table::rangeByInt32(int fieldIndex, int32_t keyMin, int32_t keyMax) {
    if (!idxInt32_ || fieldIndex != idxInt32Field_) return {};
    return idxInt32_->range(keyMin, keyMax);
}

bool Table::createStringIndex(int fieldIndex, const std::string& name) {
    if (fieldIndex < 0 || fieldIndex >= (int)schema_.fields.size()) return false;
    auto t = schema_.fields[fieldIndex].type;
    if (t != FieldType::String && t != FieldType::CharN) return false;

    idxString_ = std::make_unique<IndexString>();
    idxStringField_ = fieldIndex;
    IndexStringDesc d; d.name=name; d.fieldIndex=fieldIndex; d.path = basePath_ + "." + name + ".idx";
    idxString_->create(d);

    auto rids = scanAll();
    for (const auto& rid : rids) {
        auto rec = read(rid);
        if (!rec) continue;
        const auto& v = rec->values[fieldIndex];
        if (!v.has_value()) continue;
        const std::string& s = std::get<std::string>(v.value());
        idxString_->insert(s, rid);
    }
    return true;
}

std::vector<RID> Table::findByString(int fieldIndex, const std::string& key) {
    if (!idxString_ || fieldIndex != idxStringField_) return {};
    return idxString_->find(key);
}
std::vector<RID> Table::rangeByString(int fieldIndex, const std::string& keyMin, const std::string& keyMax) {
    if (!idxString_ || fieldIndex != idxStringField_) return {};
    return idxString_->range(keyMin, keyMax);
}

}
