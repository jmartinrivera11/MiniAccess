#include "IndexInt32.h"

namespace ma {

void IndexInt32::create(const IndexInt32Desc& d) {
    desc_ = d;
    storage_ = std::make_unique<IndexStorage>();
    storage_->create(desc_.path);
    tree_ = std::make_unique<BPlusTreeInt32>(storage_.get());
    tree_->createEmpty();
}

void IndexInt32::open(const IndexInt32Desc& d) {
    desc_ = d;
    storage_ = std::make_unique<IndexStorage>();
    storage_->open(desc_.path);
    tree_ = std::make_unique<BPlusTreeInt32>(storage_.get());
    if (storage_->rootPageId()==0) tree_->createEmpty();
}

void IndexInt32::close() {
    if (storage_) storage_->close();
    tree_.reset();
    storage_.reset();
}

void IndexInt32::insert(int32_t k, RID rid) { tree_->insert(k, rid); }
void IndexInt32::erase(int32_t k, RID rid)  { tree_->remove(k, rid); }
std::vector<RID> IndexInt32::find(int32_t k){ return tree_->find(k); }
std::vector<RID> IndexInt32::range(int32_t kmin, int32_t kmax){ return tree_->range(kmin,kmax); }

}
