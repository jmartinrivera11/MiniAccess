#pragma once
#include "Schema.h"
#include "Storage.h"
#include "Record.h"
#include "AvailList.h"
#include <string>
#include <optional>
#include "IndexInt32.h"
#include "IndexString.h"

namespace ma {

class Table {
public:
    Table() = default;
    ~Table() = default;

    void create(const std::string& basePath, const Schema& schema);
    void open(const std::string& basePath);
    void close();

    const Schema& schema() const { return schema_; }

    RID insert(const Record& rec);
    std::optional<Record> read(const RID& rid);
    bool erase(const RID& rid);
    std::optional<RID> update(const RID& rid, const Record& rec);

    size_t scanCount();
    std::vector<RID> scanAll();

    void setFitStrategy(FitStrategy s) { fit_ = s; }
    FitStrategy fitStrategy() const { return fit_; }

    bool createInt32Index(int fieldIndex, const std::string& name);
    std::vector<RID> findByInt32(int fieldIndex, int32_t key);
    std::vector<RID> rangeByInt32(int fieldIndex, int32_t keyMin, int32_t keyMax);

    bool createStringIndex(int fieldIndex, const std::string& name);
    std::vector<RID> findByString(int fieldIndex, const std::string& key);
    std::vector<RID> rangeByString(int fieldIndex, const std::string& keyMin, const std::string& keyMax);

    const ma::Schema& getSchema() const { return schema_; }

private:
    std::string basePath_;
    std::string metaPath_;
    std::string madPath_;

    Schema schema_;
    Storage storage_;
    AvailList avail_;
    FitStrategy fit_ = FitStrategy::FirstFit;

    void writeMeta();
    void readMeta();

    void rebuildAvailFromPages();

    std::optional<RID> tryInsertIntoFreeSlot(const Record& rec, const std::vector<uint8_t>& payload);
    std::optional<RID> tryInsertIntoPages(const std::vector<uint8_t>& payload);

    std::unique_ptr<IndexInt32> idxInt32_;
    int idxInt32Field_ = -1;

    std::unique_ptr<IndexString> idxString_;
    int idxStringField_ = -1;
};

}
