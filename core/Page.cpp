#include "Page.h"
#include <cstring>

namespace ma {

Page::Page() {
    bytes.resize(PAGE_SIZE, 0);
    hdr.pageId = 0;
    hdr.slotCount = 0;
    hdr.freeStart = sizeof(PageHeader);
    hdr.freeEnd = PAGE_SIZE;
    hdr.flags = 0;
    std::memcpy(bytes.data(), &hdr, sizeof(PageHeader));
}

size_t Page::freeSpace() const {
    if (hdr.freeEnd < hdr.freeStart) return 0;
    size_t cfree = static_cast<size_t>(hdr.freeEnd) - static_cast<size_t>(hdr.freeStart);
    if (cfree < sizeof(Slot)) return 0;
    return cfree - sizeof(Slot);
}

Slot Page::getSlot(uint16_t idx) const {
    if (idx >= hdr.slotCount) throw std::runtime_error("Slot index out of range");
    size_t pos = PAGE_SIZE - sizeof(Slot) * (static_cast<size_t>(idx) + 1);
    Slot s{};
    std::memcpy(&s, bytes.data() + pos, sizeof(Slot));
    return s;
}

void Page::setSlot(uint16_t idx, const Slot& s) {
    if (idx > hdr.slotCount) throw std::runtime_error("Slot set: out of range");
    size_t pos = PAGE_SIZE - sizeof(Slot) * (static_cast<size_t>(idx) + 1);
    std::memcpy(bytes.data() + pos, &s, sizeof(Slot));
}

}
