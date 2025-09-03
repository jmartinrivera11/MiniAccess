#pragma once
#include "Page.h"
#include <string>
#include <fstream>

namespace ma {

#pragma pack(push,1)
struct MadHeader {
    uint32_t magic;
    uint16_t version;
    uint32_t pageCount;
    uint8_t  reserved[54];
};
#pragma pack(pop)

class Storage {
public:
    Storage() = default;
    ~Storage();

    void create(const std::string& path);
    void open(const std::string& path);
    void close();

    uint32_t allocatePage();
    Page readPage(uint32_t pageId);
    void writePage(const Page& page);

    uint32_t pageCount() const { return header_.pageCount; }

private:
    std::fstream file_;
    std::string path_;
    MadHeader header_{};

    void writeHeader();
    void readHeader();
};

}
