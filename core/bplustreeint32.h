#pragma once
#include "IndexStorage.h"
#include "Record.h"
#include <vector>
#include <optional>

namespace ma {

#pragma pack(push,1)
struct NodeHdr {
    uint32_t pageId;
    uint8_t  isLeaf;
    uint16_t keyCount;
    uint32_t parent;
    uint32_t nextLeaf;
    uint32_t reserved;
};
#pragma pack(pop)

struct LeafEntry {
    int32_t  key;
    uint32_t ridPage;
    uint16_t ridSlot;
    uint16_t pad;
};

struct InternalEntry {
    int32_t  key;
    uint32_t child;
};

class BPlusTreeInt32 {
public:
    explicit BPlusTreeInt32(IndexStorage* storage);

    void createEmpty();
    bool isEmpty() const;

    void insert(int32_t key, RID rid);
    void remove(int32_t key, RID rid);
    std::vector<RID> find(int32_t key);
    std::vector<RID> range(int32_t keyMin, int32_t keyMax);

private:
    IndexStorage* st_{};

    Page read(uint32_t pid) { return st_->readPage(pid); }
    void write(const Page& p) { st_->writePage(p); }
    uint32_t root() const { return st_->rootPageId(); }
    void setRoot(uint32_t pid) { st_->setRootPageId(pid); }

    uint32_t ensureRootLeaf();
    uint32_t findLeafForKey(int32_t k);

    static int maxLeafEntries();
    static int maxInternalKeys();
    static int minLeafEntries();
    static int minInternalKeys();

    static int leafLowerBound(const Page& leaf, int32_t k);
    static int leafUpperBound(const Page& leaf, int32_t k);
    static int internalChildIndex(const Page& internal, int32_t k);

    static inline NodeHdr& NH(Page& p) {
        return *reinterpret_cast<NodeHdr*>(p.bytes.data());
    }
    static inline const NodeHdr& NHc(const Page& p) {
        return *reinterpret_cast<const NodeHdr*>(p.bytes.data());
    }
    static inline LeafEntry* LE(Page& p) {
        return reinterpret_cast<LeafEntry*>(p.bytes.data() + sizeof(NodeHdr));
    }
    static inline const LeafEntry* LEc(const Page& p) {
        return reinterpret_cast<const LeafEntry*>(p.bytes.data() + sizeof(NodeHdr));
    }
    static inline InternalEntry* IE(Page& p) {
        return reinterpret_cast<InternalEntry*>(p.bytes.data() + sizeof(NodeHdr));
    }
    static inline const InternalEntry* IEc(const Page& p) {
        return reinterpret_cast<const InternalEntry*>(p.bytes.data() + sizeof(NodeHdr));
    }
    static inline uint32_t* CHILD(Page& p, int maxKeys) {
        return reinterpret_cast<uint32_t*>(p.bytes.data() + sizeof(NodeHdr) + maxKeys * sizeof(InternalEntry));
    }
    static inline const uint32_t* CHILDc(const Page& p, int maxKeys) {
        return reinterpret_cast<const uint32_t*>(p.bytes.data() + sizeof(NodeHdr) + maxKeys * sizeof(InternalEntry));
    }

    void insertIntoLeaf(Page& leaf, int32_t k, RID rid);
    void splitLeafAndInsert(Page& leaf, int32_t k, RID rid);
    void insertIntoParent(uint32_t leftPid, int32_t sepKey, uint32_t rightPid);
    void splitInternalAndInsert(Page& node, int32_t sepKey, uint32_t rightPid);

    bool removeFromLeaf(Page& leaf, int32_t k, RID rid);
    void rebalanceAfterDelete(uint32_t pid);

    bool borrowFromLeftLeaf(Page& parent, int sepIdx, Page& left, Page& node);
    bool borrowFromRightLeaf(Page& parent, int sepIdx, Page& node, Page& right);
    void mergeLeaves(Page& parent, int sepIdxLeft, Page& left, Page& right);

    bool borrowFromLeftInternal(Page& parent, int sepIdx, Page& left, Page& node);
    bool borrowFromRightInternal(Page& parent, int sepIdx, Page& node, Page& right);
    void mergeInternals(Page& parent, int sepIdxLeft, Page& left, Page& right);

    static int32_t firstKeyLeaf(const Page& leaf);
    static int32_t firstKeyInternal(const Page& internal);
};

}
