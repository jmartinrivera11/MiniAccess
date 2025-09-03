#include "IndexString.h"

namespace ma {

void IndexString::create(const IndexStringDesc& d) {
    desc_ = d;
    storage_ = std::make_unique<IndexStorage>();
    storage_->create(desc_.path);
    tree_ = std::make_unique<BPlusTreeString>(storage_.get());
    tree_->createEmpty();
}

void IndexString::open(const IndexStringDesc& d) {
    desc_ = d;
    storage_ = std::make_unique<IndexStorage>();
    storage_->open(desc_.path);
    tree_ = std::make_unique<BPlusTreeString>(storage_.get());
    if (storage_->rootPageId()==0) tree_->createEmpty();
}

void IndexString::close() {
    if (storage_) storage_->close();
    tree_.reset();
    storage_.reset();
}

void IndexString::insert(const std::string& k, RID rid) {
    tree_->insert(BPlusTreeString::packKey(k), rid);
}
void IndexString::erase(const std::string& k, RID rid) {
    tree_->remove(BPlusTreeString::packKey(k), rid);
}
std::vector<RID> IndexString::find(const std::string& k) {
    return tree_->find(BPlusTreeString::packKey(k));
}
std::vector<RID> IndexString::range(const std::string& kmin, const std::string& kmax) {
    return tree_->range(BPlusTreeString::packKey(kmin), BPlusTreeString::packKey(kmax));
}

}
