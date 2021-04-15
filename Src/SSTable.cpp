#include "../Inc/SSTable.h"
#include "MurmurHash3.h"
#include <iostream>

void BloomFilter::setBit(unsigned int index) {
    unsigned int byteIndex = index / 8; //位于第几个byte？
    unsigned int bitIndex = index % 8; //第几个bit?
    set[byteIndex] |= (1 << bitIndex);
}

uint8_t BloomFilter::getBit(unsigned int index) const {
    unsigned int byteIndex = index / 8; //位于第几个byte？
    unsigned int bitIndex = index % 8; //第几个bit?
    return (set[byteIndex] >> bitIndex) & 1;
}

void BloomFilter::put(uint64_t key) {
    unsigned int hash[4] = {0};
    MurmurHash3_x64_128(&key, sizeof(key), 1, hash);
    for (unsigned int item : hash)
        setBit(item % SETSIZE);
}

void BloomFilter::printSet(unsigned int begin, unsigned int end) const {
    for (int i = begin; i < end; ++i) {
        uint8_t thisByte = set[i];
        for (int j = 0; j < 8; ++j) {
            std::cout << ((thisByte >> j) & 1) << " ";
        }
    }
    std::cout << std::endl;
};

bool BloomFilter::find(uint64_t key) const {
    unsigned int hash[4] = {0};
    MurmurHash3_x64_128(&key, sizeof(key), 1, hash);
    for (unsigned int item : hash) {
        uint8_t bit = getBit(item % SETSIZE);
        if (bit == 0)
            return false;
    }
    return true;
}

SSTable::SSTable(uint64_t timeStamp, const std::vector<uint64_t> &orderedKey, const std::vector<std::string> &value) {
    int keyNumber = orderedKey.size();
    uint32_t lastOffset = keyNumber * 12; //12 = sizeof(uint64_t) + sizeof(uint32_t),namely key + offset;
    uint32_t lastValueSize = 0;

    header.timeStamp = timeStamp;
    header.entryNumber = keyNumber;
    header.minKey = orderedKey[0];
    header.maxKey = orderedKey[keyNumber - 1];

    for (int i = 0; i < keyNumber; ++i) {
        auto thisKey = orderedKey[i];
        auto thisValue = value[i];
        bloomFilter.put(thisKey);

        auto thisOffset = lastOffset + lastValueSize;
        keyOffsetPairs.emplace_back(KeyOffsetPair(thisKey, thisOffset));
        lastOffset = thisOffset;
        lastValueSize = thisValue.size();
    }
}

void SSTable::write(const std::string &filePath) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cout << "File open failed." << std::endl;
        return;
    }
    file.write((const char *) &header.timeStamp, 8);
    file.write((const char *) &header.entryNumber, 8);
    file.write((const char *) &header.minKey, 8);
    file.write((const char *) &header.maxKey, 8);
    std::cout << "Put timeStamp " << header.timeStamp << std::endl;
    std::cout << "Put entryNumber " << header.entryNumber << std::endl;
    std::cout << "Put minKey " << header.minKey << std::endl;
    std::cout << "Put maxKey " << header.maxKey << std::endl;

    file.write((const char *) bloomFilter.set, SETSIZE);

    for (auto &pair:keyOffsetPairs) {
        std::cout << "Put key " << pair.key << std::endl;
        std::cout << "Put offset " << pair.offset << std::endl;
        file.write((const char *) &pair.key, 8);
        file.write((const char *) &pair.offset, 4);
    }
    file.close();
}

void SSTable::setHeader(uint64_t timeStamp, uint64_t entryNumber, int64_t minKey, int64_t maxKey) {
    header.timeStamp = timeStamp;
    header.entryNumber = entryNumber;
    header.minKey = minKey;
    header.maxKey = maxKey;
}

void SSTable::putKeyOffsetPair(uint64_t key, uint32_t offset) {
    keyOffsetPairs.emplace_back(KeyOffsetPair(key, offset));
}

void SSTable::setBloomFilter(void *bytes) {
    memcpy(&bloomFilter.set, bytes, SETSIZE);
}

void SSTable::readData(const std::string &filePath, std::string &dst, uint32_t thisOffset, uint32_t nextOffset) const {
    std::ifstream file(filePath);
    uint32_t basic = 10272;
    file.seekg(basic + thisOffset, std::ios::beg);
    if (nextOffset == 0) { //read until ends
        while (true) {
            char ch;
            file.get(ch);
            if (file.eof())break;
            dst.push_back(ch);
        }
    } else {
        const unsigned int size = nextOffset - thisOffset;
        for (int i = 0; i < size; ++i) {
            char ch;
            file.get(ch);
            dst.push_back(ch);
        }
    }
}

std::string SSTable::get(const std::string &filePath, uint64_t key) const {
    if (!bloomFilter.find(key)) {
        std::cout << "Key " << key << " is not in this SSTable,according to bloom filter" << std::endl;
        return "";
    }
    std::string value;
    unsigned int keyNumber = keyOffsetPairs.size();
    unsigned int l = 0, r = keyNumber - 1;
    while (l <= r) {
        unsigned int m = (l + r) >> 1;
        auto thisPair = keyOffsetPairs[m];
        auto thisKey = thisPair.key;
        std::cout << "Searching key... thisKey = " << thisKey << std::endl;
        auto thisOffset = thisPair.offset;
        std::cout << "Searching key... thisOffset = " << thisOffset << std::endl;
        uint32_t nextOffset = 0;
        if (thisKey == key) {
            if (m != keyNumber - 1)
                nextOffset = keyOffsetPairs[m + 1].offset;
            readData(filePath, value, thisOffset, nextOffset);
            break;
        } else if (thisKey > key) {
            r = m - 1;
        } else {
            l = m + 1;
        }
    }
    if (value.empty()) {
        std::cout << "Key " << key << " is not in this SSTable" << std::endl;
    } else
        std::cout << "Key " << key << " has been found in this SSTable,value is " << value << std::endl;
    return value;
}