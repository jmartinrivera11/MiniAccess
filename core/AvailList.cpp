#include "AvailList.h"
#include <algorithm>

namespace ma {

void AvailList::clear() { freelist_.clear(); }

void AvailList::add(FreeSlotRef r) {
    for (auto& x : freelist_) {
        if (x.pageId == r.pageId && x.slotId == r.slotId) {
            x.size = std::max<uint16_t>(x.size, r.size);
            return;
        }
    }
    freelist_.push_back(r);
}

std::optional<FreeSlotRef> AvailList::acquire(uint16_t needed, FitStrategy strat) {
    if (freelist_.empty()) return std::nullopt;
    size_t idx = SIZE_MAX;
    if (strat == FitStrategy::FirstFit) {
        for (size_t i = 0; i < freelist_.size(); ++i)
            if (freelist_[i].size >= needed) { idx = i; break; }
    } else if (strat == FitStrategy::BestFit) {
        uint16_t best = UINT16_MAX;
        for (size_t i = 0; i < freelist_.size(); ++i) {
            auto s = freelist_[i].size;
            if (s >= needed && s < best) { best = s; idx = i; }
        }
    } else {
        uint16_t worst = 0;
        for (size_t i = 0; i < freelist_.size(); ++i) {
            auto s = freelist_[i].size;
            if (s >= needed && s > worst) { worst = s; idx = i; }
        }
    }
    if (idx == SIZE_MAX) return std::nullopt;
    auto r = freelist_[idx];
    freelist_.erase(freelist_.begin() + idx);
    return r;
}

void AvailList::remove(uint32_t pageId, uint16_t slotId) {
    freelist_.erase(std::remove_if(freelist_.begin(), freelist_.end(),
                                   [&](const FreeSlotRef& x){ return x.pageId==pageId && x.slotId==slotId; }),
                    freelist_.end());
}

}
