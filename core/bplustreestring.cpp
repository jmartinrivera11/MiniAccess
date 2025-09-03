#include "BPlusTreeString.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace ma {

static constexpr int LEAF_ENTRY_SIZE  = sizeof(LeafEntryS);
static constexpr int INTERNAL_ENTRY_SIZE = sizeof(InternalEntryS);
static constexpr int CHILD_PTR_SIZE   = sizeof(uint32_t);

StrKey BPlusTreeString::packKey(const std::string& s) {
    StrKey k{};
    size_t L = s.size();
    if (L > (size_t)STRIDX_MAX_KEY_BYTES) L = STRIDX_MAX_KEY_BYTES;
    k.len = static_cast<uint8_t>(L);
    if (L) std::memcpy(k.bytes, s.data(), L);
    if (L < (size_t)STRIDX_MAX_KEY_BYTES) {
        std::memset(k.bytes + L, 0, STRIDX_MAX_KEY_BYTES - L);
    }
    return k;
}

int BPlusTreeString::cmpKey(const StrKey& a, const StrKey& b) {
    int n = std::min<int>(a.len, b.len);
    int c = std::memcmp(a.bytes, b.bytes, n);
    if (c != 0) return c;
    if (a.len < b.len) return -1;
    if (a.len > b.len) return  1;
    return 0;
}

int BPlusTreeString::maxLeafEntries() {
    return (PAGE_SIZE - (int)sizeof(NodeHdrS)) / LEAF_ENTRY_SIZE;
}
int BPlusTreeString::maxInternalKeys() {
    int M = (PAGE_SIZE - (int)sizeof(NodeHdrS) - (int)sizeof(uint32_t)) /
            (INTERNAL_ENTRY_SIZE + (int)sizeof(uint32_t));
    return std::max(3, M-1);
}
int BPlusTreeString::minLeafEntries()  { return std::max(1, maxLeafEntries() / 2); }
int BPlusTreeString::minInternalKeys() { return std::max(1, maxInternalKeys() / 2); }

BPlusTreeString::BPlusTreeString(IndexStorage* storage): st_(storage) {}
bool BPlusTreeString::isEmpty() const { return st_->rootPageId() == 0; }

uint32_t BPlusTreeString::ensureRootLeaf() {
    if (!isEmpty()) return root();
    Page leaf; leaf.hdr.pageId = st_->allocatePage();
    NodeHdrS nh{}; nh.pageId=leaf.hdr.pageId; nh.isLeaf=1; nh.keyCount=0; nh.parent=0; nh.nextLeaf=0;
    std::memcpy(leaf.bytes.data(), &nh, sizeof(NodeHdrS));
    write(leaf);
    setRoot(leaf.hdr.pageId);
    return leaf.hdr.pageId;
}

void BPlusTreeString::createEmpty() { ensureRootLeaf(); }

uint32_t BPlusTreeString::findLeafForKey(const StrKey& k) {
    uint32_t pid = ensureRootLeaf();
    Page p = read(pid);
    while (!NHc(p).isLeaf) {
        int M = maxInternalKeys();
        const auto* keys = IEc(p);
        const auto* ch = CHILDc(p, M);
        int kc = NHc(p).keyCount;
        (void)keys; (void)kc;
        int i = internalChildIndex(p, k);
        pid = ch[i];
        p = read(pid);
    }
    return pid;
}

int BPlusTreeString::leafLowerBound(const Page& leaf, const StrKey& k) {
    int lo = 0, hi = NHc(leaf).keyCount;
    const auto* a = LEc(leaf);
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cmpKey(a[mid].key, k) < 0) lo = mid + 1; else hi = mid;
    }
    return lo;
}
int BPlusTreeString::leafUpperBound(const Page& leaf, const StrKey& k) {
    int lo = 0, hi = NHc(leaf).keyCount;
    const auto* a = LEc(leaf);
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cmpKey(a[mid].key, k) <= 0) lo = mid + 1; else hi = mid;
    }
    return lo;
}
int BPlusTreeString::internalChildIndex(const Page& internal, const StrKey& k) {
    int kc = NHc(internal).keyCount;
    const auto* a = IEc(internal);
    int i = 0;
    while (i < kc && cmpKey(a[i].key, k) <= 0) ++i;
    return i;
}

void BPlusTreeString::insert(const StrKey& key, RID rid) {
    uint32_t leafPid = findLeafForKey(key);
    Page leaf = read(leafPid);
    int maxE = maxLeafEntries();
    if (NHc(leaf).keyCount < maxE) {
        insertIntoLeaf(leaf, key, rid);
        write(leaf);
    } else {
        splitLeafAndInsert(leaf, key, rid);
    }
}

void BPlusTreeString::insertIntoLeaf(Page& leaf, const StrKey& k, RID rid) {
    int pos = leafUpperBound(leaf, k);
    int kc = NHc(leaf).keyCount;
    auto* a = LE(leaf);
    for (int i = kc; i > pos; --i) a[i] = a[i-1];
    a[pos].key = k; a[pos].ridPage = rid.pageId; a[pos].ridSlot = rid.slotId; a[pos].pad=0;
    NH(leaf).keyCount = kc + 1;
}

void BPlusTreeString::splitLeafAndInsert(Page& leaf, const StrKey& k, RID rid) {
    Page right; right.hdr.pageId = st_->allocatePage();
    NodeHdrS rnh{}; rnh.pageId=right.hdr.pageId; rnh.isLeaf=1; rnh.keyCount=0; rnh.parent=NHc(leaf).parent; rnh.nextLeaf=NHc(leaf).nextLeaf;
    std::memcpy(right.bytes.data(), &rnh, sizeof(NodeHdrS));

    int kc = NHc(leaf).keyCount;
    int move = kc/2;
    auto* la = LE(leaf);
    auto* ra = LE(right);
    for (int i=0;i<move;i++) ra[i] = la[kc - move + i];
    NH(right).keyCount = move;
    NH(leaf).keyCount = kc - move;
    NH(leaf).nextLeaf = right.hdr.pageId;

    if (cmpKey(k, LEc(right)[0].key) >= 0) insertIntoLeaf(right, k, rid);
    else insertIntoLeaf(leaf, k, rid);

    write(leaf); write(right);

    StrKey sepKey = LEc(right)[0].key;
    insertIntoParent(leaf.hdr.pageId, sepKey, right.hdr.pageId);
}

void BPlusTreeString::insertIntoParent(uint32_t leftPid, const StrKey& sepKey, uint32_t rightPid) {
    if (leftPid == root()) {
        Page rootp; rootp.hdr.pageId = st_->allocatePage();
        NodeHdrS nh{}; nh.pageId=rootp.hdr.pageId; nh.isLeaf=0; nh.keyCount=1; nh.parent=0; nh.nextLeaf=0;
        std::memcpy(rootp.bytes.data(), &nh, sizeof(NodeHdrS));

        int M = maxInternalKeys();
        auto* keys = IE(rootp);
        auto* ch = CHILD(rootp, M);
        keys[0].key = sepKey;
        ch[0] = leftPid;
        ch[1] = rightPid;

        Page L = read(leftPid);  NH(L).parent = rootp.hdr.pageId; write(L);
        Page R = read(rightPid); NH(R).parent = rootp.hdr.pageId; write(R);

        write(rootp);
        setRoot(rootp.hdr.pageId);
        return;
    }

    Page left = read(leftPid);
    uint32_t parentPid = NHc(left).parent;
    Page parent = read(parentPid);

    int M = maxInternalKeys();
    auto* keys = IE(parent);
    auto* ch = CHILD(parent, M);
    int kc = NHc(parent).keyCount;

    int iChild = 0; while (iChild <= kc && ch[iChild] != leftPid) ++iChild;
    if (iChild > kc) throw std::runtime_error("B+ parent child not found");

    for (int i = kc; i > iChild; --i) keys[i] = keys[i-1];
    for (int i = kc+1; i > iChild+1; --i) ch[i] = ch[i-1];
    keys[iChild].key = sepKey;
    ch[iChild+1] = rightPid;
    NH(parent).keyCount = kc + 1;

    if (NHc(parent).keyCount <= maxInternalKeys()) {
        write(parent);
        Page R = read(rightPid); NH(R).parent = parent.hdr.pageId; write(R);
    } else {
        splitInternalAndInsert(parent);
        Page R = read(rightPid); write(R);
    }
}

void BPlusTreeString::splitInternalAndInsert(Page& node) {
    int kc = NHc(node).keyCount;
    int mid = kc/2;
    int M = maxInternalKeys();

    Page right; right.hdr.pageId = st_->allocatePage();
    NodeHdrS nh{}; nh.pageId=right.hdr.pageId; nh.isLeaf=0; nh.keyCount=0; nh.parent=NHc(node).parent; nh.nextLeaf=0;
    std::memcpy(right.bytes.data(), &nh, sizeof(NodeHdrS));

    auto* keysL = IE(node);
    auto* chL   = CHILD(node, M);
    auto* keysR = IE(right);
    auto* chR   = CHILD(right, M);

    StrKey promote = keysL[mid].key;

    int rkeys = kc - (mid + 1);
    for (int i=0;i<rkeys;i++) keysR[i] = keysL[mid+1 + i];
    for (int i=0;i<rkeys+1;i++) chR[i] = chL[mid+1 + i];

    NH(node).keyCount = mid;

    for (int i=0;i<rkeys+1;i++) {
        Page c = read(chR[i]); NH(c).parent = right.hdr.pageId; write(c);
    }

    NH(right).keyCount = rkeys;
    write(node); write(right);

    insertIntoParent(node.hdr.pageId, promote, right.hdr.pageId);
}

std::vector<RID> BPlusTreeString::find(const StrKey& key) {
    uint32_t leafPid = findLeafForKey(key);
    Page leaf = read(leafPid);
    int lo = leafLowerBound(leaf, key);
    int hi = leafUpperBound(leaf, key);
    std::vector<RID> out;
    const auto* a = LEc(leaf);
    for (int i=lo;i<hi;i++) out.push_back(RID{a[i].ridPage, a[i].ridSlot});
    return out;
}

std::vector<RID> BPlusTreeString::range(const StrKey& keyMin, const StrKey& keyMax) {
    if (cmpKey(keyMax, keyMin) < 0) return {};
    std::vector<RID> out;
    uint32_t pid = findLeafForKey(keyMin);
    Page leaf = read(pid);
    while (true) {
        const auto* a = LEc(leaf);
        int kc = NHc(leaf).keyCount;
        for (int i=0;i<kc;i++) {
            if (cmpKey(a[i].key, keyMin) < 0) continue;
            if (cmpKey(a[i].key, keyMax) > 0) return out;
            out.push_back(RID{a[i].ridPage, a[i].ridSlot});
        }
        if (NHc(leaf).nextLeaf == 0) break;
        leaf = read(NHc(leaf).nextLeaf);
    }
    return out;
}

bool BPlusTreeString::removeFromLeaf(Page& leaf, const StrKey& k, RID rid) {
    auto* a = LE(leaf);
    int kc = NHc(leaf).keyCount;
    for (int i=0;i<kc;i++) {
        if (cmpKey(a[i].key, k)==0 && a[i].ridPage==rid.pageId && a[i].ridSlot==rid.slotId) {
            for (int j=i+1;j<kc;j++) a[j-1]=a[j];
            NH(leaf).keyCount = kc-1;
            return true;
        }
    }
    return false;
}

void BPlusTreeString::remove(const StrKey& key, RID rid) {
    uint32_t leafPid = findLeafForKey(key);
    Page leaf = read(leafPid);
    if (removeFromLeaf(leaf, key, rid)) {
        write(leaf);
        rebalanceAfterDelete(leafPid);
    }
}

StrKey BPlusTreeString::firstKeyLeaf(const Page& leaf) {
    if (NHc(leaf).keyCount == 0) return StrKey{};
    return LEc(leaf)[0].key;
}
StrKey BPlusTreeString::firstKeyInternal(const Page& internal) {
    if (NHc(internal).keyCount == 0) return StrKey{};
    return IEc(internal)[0].key;
}

void BPlusTreeString::rebalanceAfterDelete(uint32_t pid) {
    Page node = read(pid);

    if (pid == root()) {
        if (!NHc(node).isLeaf && NHc(node).keyCount == 0) {
            int M = maxInternalKeys();
            auto* ch = CHILD(node, M);
            uint32_t newRootPid = ch[0];
            Page child = read(newRootPid);
            NH(child).parent = 0;
            write(child);
            setRoot(newRootPid);
        }
        return;
    }

    int minReq = NHc(node).isLeaf ? minLeafEntries() : minInternalKeys();
    if (NHc(node).keyCount >= minReq) return;

    uint32_t parentPid = NHc(node).parent;
    Page parent = read(parentPid);
    int M = maxInternalKeys();
    auto* ch = CHILD(parent, M);
    int kcP = NHc(parent).keyCount;
    int idx = 0; while (idx <= kcP && ch[idx] != pid) ++idx;
    if (idx > kcP) throw std::runtime_error("rebalance: child not found");

    if (NHc(node).isLeaf) {
        if (idx > 0) {
            Page left = read(ch[idx-1]);
            if (NHc(left).keyCount > minLeafEntries()) {
                if (borrowFromLeftLeaf(parent, idx-1, left, node)) { write(parent); write(left); write(node); return; }
            }
        }
        if (idx < kcP) {
            Page right = read(ch[idx+1]);
            if (NHc(right).keyCount > minLeafEntries()) {
                if (borrowFromRightLeaf(parent, idx, node, right)) { write(parent); write(right); write(node); return; }
            }
        }
        if (idx > 0) {
            Page left = read(ch[idx-1]);
            mergeLeaves(parent, idx-1, left, node);
            write(parent); write(left);
            rebalanceAfterDelete(parentPid);
            return;
        } else {
            Page right = read(ch[idx+1]);
            mergeLeaves(parent, idx, node, right);
            write(parent); write(node);
            rebalanceAfterDelete(parentPid);
            return;
        }
    } else {
        if (idx > 0) {
            Page left = read(ch[idx-1]);
            if (NHc(left).keyCount > minInternalKeys()) {
                if (borrowFromLeftInternal(parent, idx-1, left, node)) { write(parent); write(left); write(node); return; }
            }
        }
        if (idx < kcP) {
            Page right = read(ch[idx+1]);
            if (NHc(right).keyCount > minInternalKeys()) {
                if (borrowFromRightInternal(parent, idx, node, right)) { write(parent); write(right); write(node); return; }
            }
        }
        if (idx > 0) {
            Page left = read(ch[idx-1]);
            mergeInternals(parent, idx-1, left, node);
            write(parent); write(left);
            rebalanceAfterDelete(parentPid);
            return;
        } else {
            Page right = read(ch[idx+1]);
            mergeInternals(parent, idx, node, right);
            write(parent); write(node);
            rebalanceAfterDelete(parentPid);
            return;
        }
    }
}

bool BPlusTreeString::borrowFromLeftLeaf(Page& parent, int sepIdx, Page& left, Page& node) {
    int kcL = NHc(left).keyCount;
    int kcN = NHc(node).keyCount;
    if (kcL <= minLeafEntries()) return false;
    auto* aL = LE(left);
    auto* aN = LE(node);
    for (int i=kcN; i>0; --i) aN[i] = aN[i-1];
    aN[0] = aL[kcL-1];
    NH(node).keyCount = kcN + 1;
    NH(left).keyCount = kcL - 1;
    IE(parent)[sepIdx].key = firstKeyLeaf(node);
    return true;
}
bool BPlusTreeString::borrowFromRightLeaf(Page& parent, int sepIdx, Page& node, Page& right) {
    int kcR = NHc(right).keyCount;
    int kcN = NHc(node).keyCount;
    if (kcR <= minLeafEntries()) return false;
    auto* aR = LE(right);
    auto* aN = LE(node);
    aN[kcN] = aR[0];
    for (int i=1;i<kcR;i++) aR[i-1] = aR[i];
    NH(node).keyCount = kcN + 1;
    NH(right).keyCount = kcR - 1;
    IE(parent)[sepIdx].key = firstKeyLeaf(right);
    return true;
}
void BPlusTreeString::mergeLeaves(Page& parent, int sepIdxLeft, Page& left, Page& right) {
    int kcL = NHc(left).keyCount;
    int kcR = NHc(right).keyCount;
    auto* aL = LE(left);
    const auto* aR = LEc(right);
    for (int i=0;i<kcR;i++) aL[kcL+i] = aR[i];
    NH(left).keyCount = kcL + kcR;
    NH(left).nextLeaf = NHc(right).nextLeaf;

    int M = maxInternalKeys();
    auto* keysP = IE(parent);
    auto* chP = CHILD(parent, M);
    int kcP = NHc(parent).keyCount;
    for (int i=sepIdxLeft; i<kcP-1; ++i) keysP[i] = keysP[i+1];
    for (int i=sepIdxLeft+1; i<kcP; ++i) chP[i] = chP[i+1];
    NH(parent).keyCount = kcP - 1;
}

bool BPlusTreeString::borrowFromLeftInternal(Page& parent, int sepIdx, Page& left, Page& node) {
    int kcL = NHc(left).keyCount;
    int kcN = NHc(node).keyCount;
    if (kcL <= minInternalKeys()) return false;

    int M = maxInternalKeys();
    auto* keysL = IE(left);
    auto* chL   = CHILD(left, M);
    auto* keysN = IE(node);
    auto* chN   = CHILD(node, M);

    for (int i=kcN; i>0; --i) keysN[i] = keysN[i-1];
    for (int i=kcN+1; i>0; --i) chN[i] = chN[i-1];

    keysN[0].key = IEc(parent)[sepIdx].key;
    chN[0] = chL[kcL];

    IE(parent)[sepIdx].key = keysL[kcL-1].key;

    NH(left).keyCount = kcL - 1;
    NH(node).keyCount = kcN + 1;

    Page movedChild = read(chN[0]); NH(movedChild).parent = node.hdr.pageId; write(movedChild);
    return true;
}
bool BPlusTreeString::borrowFromRightInternal(Page& parent, int sepIdx, Page& node, Page& right) {
    int kcR = NHc(right).keyCount;
    int kcN = NHc(node).keyCount;
    if (kcR <= minInternalKeys()) return false;

    int M = maxInternalKeys();
    auto* keysR = IE(right);
    auto* chR   = CHILD(right, M);
    auto* keysN = IE(node);
    auto* chN   = CHILD(node, M);

    keysN[kcN].key = IEc(parent)[sepIdx].key;
    chN[kcN+1] = chR[0];
    IE(parent)[sepIdx].key = keysR[0].key;

    for (int i=1;i<kcR;i++) keysR[i-1] = keysR[i];
    for (int i=1;i<kcR+1;i++) chR[i-1] = chR[i];

    NH(right).keyCount = kcR - 1;
    NH(node).keyCount  = kcN + 1;

    Page movedChild = read(chN[kcN+1]); NH(movedChild).parent = node.hdr.pageId; write(movedChild);
    return true;
}
void BPlusTreeString::mergeInternals(Page& parent, int sepIdxLeft, Page& left, Page& right) {
    int kcL = NHc(left).keyCount;
    int kcR = NHc(right).keyCount;
    int M = maxInternalKeys();

    auto* keysL = IE(left);
    auto* chL   = CHILD(left, M);
    const auto* keysR = IEc(right);
    const auto* chR   = CHILDc(right, M);

    keysL[kcL].key = IEc(parent)[sepIdxLeft].key;
    chL[kcL+1] = chR[0];

    for (int i=0;i<kcR;i++) keysL[kcL+1+i] = keysR[i];
    for (int i=1;i<kcR+1;i++) chL[kcL+1+i] = chR[i];

    for (int i=0;i<kcR+1;i++) {
        Page c = read(chR[i]); NH(c).parent = left.hdr.pageId; write(c);
    }

    NH(left).keyCount = kcL + 1 + kcR;

    auto* keysP = IE(parent);
    auto* chP   = CHILD(parent, M);
    int kcP = NHc(parent).keyCount;
    for (int i=sepIdxLeft; i<kcP-1; ++i) keysP[i] = keysP[i+1];
    for (int i=sepIdxLeft+1; i<kcP;   ++i) chP[i]   = chP[i+1];
    NH(parent).keyCount = kcP - 1;
}

}
