#include "IndexStorage.h"
#include <stdexcept>
#include <cstring>
#include <vector>

namespace ma {

static constexpr uint32_t IDX_MAGIC = 0x31584449u;

IndexStorage::~IndexStorage() { close(); }

void IndexStorage::create(const std::string& path) {
    close();
    path_ = path;
    file_.open(path_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_) throw std::runtime_error("Idx: cannot create " + path_);
    header_.magic = IDX_MAGIC;
    header_.version = 1;
    header_.pageCount = 1;
    header_.rootPageId = 0;
    header_.keyKind = 0;
    header_.keyBytes = 0;
    std::memset(header_.reserved, 0, sizeof(header_.reserved));

    std::vector<uint8_t> p0(PAGE_SIZE, 0);
    std::memcpy(p0.data(), &header_, sizeof(IdxHeader));
    file_.write(reinterpret_cast<const char*>(p0.data()), PAGE_SIZE);
    file_.flush();
    file_.close();

    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_) throw std::runtime_error("Idx: cannot reopen " + path_);
}

void IndexStorage::open(const std::string& path) {
    close();
    path_ = path;
    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_) throw std::runtime_error("Idx: cannot open " + path_);
    readHeader();
}

void IndexStorage::close() {
    if (file_.is_open()) {
        std::vector<uint8_t> p0(PAGE_SIZE, 0);
        std::memcpy(p0.data(), &header_, sizeof(IdxHeader));
        file_.seekp(0, std::ios::beg);
        file_.write(reinterpret_cast<const char*>(p0.data()), PAGE_SIZE);
        file_.flush();
        file_.close();
    }
}

void IndexStorage::writeHeader() {
    std::vector<uint8_t> p0(PAGE_SIZE, 0);
    std::memcpy(p0.data(), &header_, sizeof(IdxHeader));
    file_.seekp(0, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(p0.data()), PAGE_SIZE);
    file_.flush();
}

void IndexStorage::readHeader() {
    std::vector<uint8_t> p0(PAGE_SIZE, 0);
    file_.seekg(0, std::ios::beg);
    file_.read(reinterpret_cast<char*>(p0.data()), PAGE_SIZE);
    if (!file_) throw std::runtime_error("Idx: read header failed");
    std::memcpy(&header_, p0.data(), sizeof(IdxHeader));
    if (header_.magic != IDX_MAGIC || header_.version != 1)
        throw std::runtime_error("Idx: invalid header");
}

uint32_t IndexStorage::allocatePage() {
    uint32_t newPid = header_.pageCount;
    std::vector<uint8_t> zero(PAGE_SIZE, 0);
    file_.seekp(static_cast<std::streamoff>(newPid) * PAGE_SIZE, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(zero.data()), PAGE_SIZE);
    file_.flush();
    if (!file_) throw std::runtime_error("Idx: allocatePage failed");
    header_.pageCount++;
    writeHeader();
    return newPid;
}

Page IndexStorage::readPage(uint32_t pageId) {
    if (pageId >= header_.pageCount) throw std::runtime_error("Idx: readPage out of range");
    Page p;
    file_.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE, std::ios::beg);
    file_.read(reinterpret_cast<char*>(p.bytes.data()), PAGE_SIZE);
    if (!file_) throw std::runtime_error("Idx: read page failed");
    p.hdr.pageId = pageId;
    return p;
}

void IndexStorage::writePage(const Page& page) {
    if (page.hdr.pageId >= header_.pageCount) throw std::runtime_error("Idx: writePage out of range");
    file_.seekp(static_cast<std::streamoff>(page.hdr.pageId) * PAGE_SIZE, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(page.bytes.data()), PAGE_SIZE);
    file_.flush();
    if (!file_) throw std::runtime_error("Idx: write page failed");
}

void IndexStorage::setRootPageId(uint32_t pid) {
    header_.rootPageId = pid;
    writeHeader();
}

void IndexStorage::setKeyMeta(uint16_t kind, uint16_t bytes) {
    header_.keyKind = kind;
    header_.keyBytes = bytes;
    writeHeader();
}

}
