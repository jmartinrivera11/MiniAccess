#pragma once
#include "BPlusTreeInt32.h"
#include <memory>

namespace ma {

struct IndexInt32Desc {
    std::string name;
    int fieldIndex;
    std::string path;
};

class IndexInt32 {
public:
    IndexInt32() = default;

    void create(const IndexInt32Desc& d);
    void open(const IndexInt32Desc& d);
    void close();

    void insert(int32_t k, RID rid);
    void erase(int32_t k, RID rid);
    std::vector<RID> find(int32_t k);
    std::vector<RID> range(int32_t kmin, int32_t kmax);

    const IndexInt32Desc& desc() const { return desc_; }

private:
    IndexInt32Desc desc_{};
    std::unique_ptr<IndexStorage> storage_;
    std::unique_ptr<BPlusTreeInt32> tree_;
};

}
