#pragma once
#include "BPlusTreeString.h"
#include <memory>

namespace ma {

struct IndexStringDesc {
    std::string name;
    int fieldIndex;
    std::string path;
};

class IndexString {
public:
    IndexString() = default;

    void create(const IndexStringDesc& d);
    void open(const IndexStringDesc& d);
    void close();

    void insert(const std::string& k, RID rid);
    void erase(const std::string& k, RID rid);
    std::vector<RID> find(const std::string& k);
    std::vector<RID> range(const std::string& kmin, const std::string& kmax);

    const IndexStringDesc& desc() const { return desc_; }

private:
    IndexStringDesc desc_{};
    std::unique_ptr<IndexStorage> storage_;
    std::unique_ptr<BPlusTreeString> tree_;
};

}
