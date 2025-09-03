#pragma once
#include "Page.h"
#include <string>
#include <fstream>

namespace ma {

#pragma pack(push,1)
struct IdxHeader {
    uint32_t magic;
    uint16_t version;
    uint32_t pageCount;
    uint32_t rootPageId;
    uint16_t keyKind;
    uint16_t keyBytes;
    uint8_t  reserved[44];
};
#pragma pack(pop)

class IndexStorage {
public:
    IndexStorage() = default;
    ~IndexStorage();

    void create(const std::string& path);
    void open(const std::string& path);
    void close();

    uint32_t allocatePage();
    Page readPage(uint32_t pageId);
    void writePage(const Page& page);

    uint32_t pageCount() const { return header_.pageCount; }
    uint32_t rootPageId() const { return header_.rootPageId; }
    void setRootPageId(uint32_t pid);

    uint16_t keyKind() const { return header_.keyKind; }
    uint16_t keyBytes() const { return header_.keyBytes; }
    void setKeyMeta(uint16_t kind, uint16_t bytes);

private:
    std::fstream file_;
    std::string path_;
    IdxHeader header_{};

    void writeHeader();
    void readHeader();
};

}
