#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include "Page.h"

namespace ma {

enum class FitStrategy { FirstFit, BestFit, WorstFit };

struct FreeSlotRef {
    uint32_t pageId;
    uint16_t slotId;
    uint16_t size;
};

class AvailList {
public:
    void clear();
    void add(FreeSlotRef r);
    std::optional<FreeSlotRef> acquire(uint16_t needed, FitStrategy strat);
    void remove(uint32_t pageId, uint16_t slotId);
    size_t size() const { return freelist_.size(); }

private:
    std::vector<FreeSlotRef> freelist_;
};

}
