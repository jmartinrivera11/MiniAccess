#pragma once
#include "IndexStorage.h"
#include "Record.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <cstring>

namespace ma {

constexpr int STRIDX_MAX_KEY_BYTES = 64;

#pragma pack(push,1)
struct NodeHdrS {
    uint32_t pageId;
    uint8_t  isLeaf;
    uint16_t keyCount;
    uint32_t parent;
    uint32_t nextLeaf;
    uint32_t reserved;
};

struct StrKey {
    uint8_t len;
    char    bytes[STRIDX_MAX_KEY_BYTES];
};

struct LeafEntryS {
    StrKey   key;
    uint32_t ridPage;
    uint16_t ridSlot;
    uint16_t pad;
};

struct InternalEntryS {
    StrKey   key;
    uint32_t child;
};
#pragma pack(pop)

class BPlusTreeString {
public:
    explicit BPlusTreeString(IndexStorage* storage);

    void createEmpty();
    bool isEmpty() const;

    void insert(const StrKey& key, RID rid);
    void remove(const StrKey& key, RID rid);
    std::vector<RID> find(const StrKey& key);
    std::vector<RID> range(const StrKey& keyMin, const StrKey& keyMax);

    static StrKey packKey(const std::string& s);

private:
    IndexStorage* st_{};

    Page read(uint32_t pid) { return st_->readPage(pid); }
    void write(const Page& p) { st_->writePage(p); }
    uint32_t root() const { return st_->rootPageId(); }
    void setRoot(uint32_t pid) { st_->setRootPageId(pid); }

    uint32_t ensureRootLeaf();
    uint32_t findLeafForKey(const StrKey& k);

    static int maxLeafEntries();
    static int maxInternalKeys();
    static int minLeafEntries();
    static int minInternalKeys();

    static int cmpKey(const StrKey& a, const StrKey& b);

    static int leafLowerBound(const Page& leaf, const StrKey& k);
    static int leafUpperBound(const Page& leaf, const StrKey& k);
    static int internalChildIndex(const Page& internal, const StrKey& k);

    static inline NodeHdrS& NH(Page& p) {
        return *reinterpret_cast<NodeHdrS*>(p.bytes.data());
    }
    static inline const NodeHdrS& NHc(const Page& p) {
        return *reinterpret_cast<const NodeHdrS*>(p.bytes.data());
    }
    static inline LeafEntryS* LE(Page& p) {
        return reinterpret_cast<LeafEntryS*>(p.bytes.data() + sizeof(NodeHdrS));
    }
    static inline const LeafEntryS* LEc(const Page& p) {
        return reinterpret_cast<const LeafEntryS*>(p.bytes.data() + sizeof(NodeHdrS));
    }
    static inline InternalEntryS* IE(Page& p) {
        return reinterpret_cast<InternalEntryS*>(p.bytes.data() + sizeof(NodeHdrS));
    }
    static inline const InternalEntryS* IEc(const Page& p) {
        return reinterpret_cast<const InternalEntryS*>(p.bytes.data() + sizeof(NodeHdrS));
    }
    static inline uint32_t* CHILD(Page& p, int maxKeys) {
        return reinterpret_cast<uint32_t*>(p.bytes.data() + sizeof(NodeHdrS) + maxKeys * sizeof(InternalEntryS));
    }
    static inline const uint32_t* CHILDc(const Page& p, int maxKeys) {
        return reinterpret_cast<const uint32_t*>(p.bytes.data() + sizeof(NodeHdrS) + maxKeys * sizeof(InternalEntryS));
    }

    void insertIntoLeaf(Page& leaf, const StrKey& k, RID rid);
    void splitLeafAndInsert(Page& leaf, const StrKey& k, RID rid);
    void insertIntoParent(uint32_t leftPid, const StrKey& sepKey, uint32_t rightPid);
    void splitInternalAndInsert(Page& node);

    bool removeFromLeaf(Page& leaf, const StrKey& k, RID rid);
    void rebalanceAfterDelete(uint32_t pid);

    bool borrowFromLeftLeaf(Page& parent, int sepIdx, Page& left, Page& node);
    bool borrowFromRightLeaf(Page& parent, int sepIdx, Page& node, Page& right);
    void mergeLeaves(Page& parent, int sepIdxLeft, Page& left, Page& right);

    bool borrowFromLeftInternal(Page& parent, int sepIdx, Page& left, Page& node);
    bool borrowFromRightInternal(Page& parent, int sepIdx, Page& node, Page& right);
    void mergeInternals(Page& parent, int sepIdxLeft, Page& left, Page& right);

    static StrKey firstKeyLeaf(const Page& leaf);
    static StrKey firstKeyInternal(const Page& internal);
};

}
