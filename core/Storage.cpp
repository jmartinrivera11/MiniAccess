#include "Storage.h"
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace ma {

static constexpr uint32_t MAD_MAGIC = 0x3144414Du;

Storage::~Storage() {
    close();
}

void Storage::create(const std::string& path) {
    close();
    path_ = path;
    file_.open(path_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_) throw std::runtime_error("Cannot create file: " + path_);
    header_.magic = MAD_MAGIC;
    header_.version = 1;
    header_.pageCount = 1;
    std::memset(header_.reserved, 0, sizeof(header_.reserved));

    Page p0;
    p0.hdr.pageId = 0;
    std::memcpy(p0.bytes.data(), &header_, sizeof(MadHeader));
    file_.write(reinterpret_cast<const char*>(p0.bytes.data()), PAGE_SIZE);
    file_.flush();
    file_.close();

    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_) throw std::runtime_error("Cannot reopen created file");
}

void Storage::open(const std::string& path) {
    close();
    path_ = path;
    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_) throw std::runtime_error("Cannot open file: " + path_);
    readHeader();
}

void Storage::close() {
    if (file_.is_open()) {
        file_.seekp(0, std::ios::beg);
        Page p0;
        p0.hdr.pageId = 0;
        std::memcpy(p0.bytes.data(), &header_, sizeof(MadHeader));
        file_.write(reinterpret_cast<const char*>(p0.bytes.data()), PAGE_SIZE);
        file_.flush();
        file_.close();
    }
}

void Storage::writeHeader() {
    file_.seekp(0, std::ios::beg);
    Page p0;
    p0.hdr.pageId = 0;
    std::memcpy(p0.bytes.data(), &header_, sizeof(MadHeader));
    file_.write(reinterpret_cast<const char*>(p0.bytes.data()), PAGE_SIZE);
    file_.flush();
}

void Storage::readHeader() {
    file_.seekg(0, std::ios::beg);
    Page p0;
    file_.read(reinterpret_cast<char*>(p0.bytes.data()), PAGE_SIZE);
    if (!file_) throw std::runtime_error("Failed to read MAD header page");
    std::memcpy(&header_, p0.bytes.data(), sizeof(MadHeader));
    if (header_.magic != MAD_MAGIC || header_.version != 1)
        throw std::runtime_error("Invalid MAD file header");
}

uint32_t Storage::allocatePage() {
    Page p;
    p.hdr.pageId = header_.pageCount;
    std::memcpy(p.bytes.data(), &p.hdr, sizeof(PageHeader));
    file_.seekp(static_cast<std::streamoff>(header_.pageCount) * PAGE_SIZE, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(p.bytes.data()), PAGE_SIZE);
    file_.flush();
    if (!file_) throw std::runtime_error("Failed to allocate page");
    header_.pageCount++;
    writeHeader();
    return p.hdr.pageId;
}

Page Storage::readPage(uint32_t pageId) {
    if (pageId >= header_.pageCount) throw std::runtime_error("readPage: out of range");
    Page p;
    file_.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE, std::ios::beg);
    file_.read(reinterpret_cast<char*>(p.bytes.data()), PAGE_SIZE);
    if (!file_) throw std::runtime_error("Failed to read page");
    std::memcpy(&p.hdr, p.bytes.data(), sizeof(PageHeader));
    p.hdr.pageId = pageId;
    return p;
}

void Storage::writePage(const Page& page) {
    if (page.hdr.pageId >= header_.pageCount) throw std::runtime_error("writePage: out of range");
    Page p = page;
    std::memcpy(p.bytes.data(), &p.hdr, sizeof(PageHeader)); // sync header en bytes
    file_.seekp(static_cast<std::streamoff>(p.hdr.pageId) * PAGE_SIZE, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(p.bytes.data()), PAGE_SIZE);
    file_.flush();
    if (!file_) throw std::runtime_error("Failed to write page");
}

}
