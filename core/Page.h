#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace ma {

constexpr uint32_t PAGE_SIZE = 4096;

#pragma pack(push,1)
struct PageHeader {
    uint32_t pageId;
    uint16_t slotCount;
    uint16_t freeStart;
    uint16_t freeEnd;
    uint8_t  flags;
};

struct Slot {
    uint16_t offset;
    uint16_t length;
};
#pragma pack(pop)

inline bool slotIsFree(const Slot& s) { return (s.length & 0x8000u) != 0; }
inline uint16_t slotLen(const Slot& s) { return (s.length & 0x7FFFu); }
inline void markSlotFree(Slot& s) { s.length = s.length | 0x8000u; }
inline void markSlotUsed(Slot& s) { s.length = s.length & 0x7FFFu; }

struct Page {
    PageHeader hdr;
    std::vector<uint8_t> bytes;

    Page();
    size_t freeSpace() const;
    Slot getSlot(uint16_t idx) const;
    void setSlot(uint16_t idx, const Slot& s);
};

}
