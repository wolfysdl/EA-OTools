#pragma once
#include <cstring>
#include <string>

class BinaryBuffer {
    static const unsigned int DEFAULT_START_CAPACITY = 4096;
    unsigned char *mData;
    unsigned int mSize;
    unsigned int mCapacity;
    unsigned char *mCurrent;

    void PutData(void *data, unsigned int size) {
        if ((Position() + size) > mCapacity) {
            unsigned int newCapacity = mCapacity * 2;
            unsigned char *newData = new unsigned char[newCapacity];
            memcpy(newData, mData, mCapacity);
            delete[] mData;
            mCapacity = newCapacity;
        }
        memcpy(mCurrent, data, size);
        mCurrent += size;
        if (mCurrent > (mData + mSize))
            mSize = mCurrent - mData;
    }
public:
    BinaryBuffer() {
        mData = new unsigned char[DEFAULT_START_CAPACITY];
        mSize = 0;
        mCapacity = DEFAULT_START_CAPACITY;
        mCurrent = mData;
    }

    BinaryBuffer(unsigned int startingCapacity) {
        mData = new unsigned char[startingCapacity];
        mSize = 0;
        mCapacity = startingCapacity;
        mCurrent = mData;
    }

    ~BinaryBuffer() {
        delete[] mData;
    }

    unsigned int Position() {
        return mCurrent - mData;
    }

    unsigned int Size() {
        return mSize;
    }

    unsigned char *Data() {
        return mData;
    }

    void MoveTo(unsigned int position) {
        if (position < mSize)
            mCurrent = mData + position;
        else
            mCurrent = mData + mSize;
    }

    template<typename T>
    void Put(T const &value) {
        PutData(&value, sizeof(T));
    }

    void Put(std::string const &str) {
        PutData((void *)str.c_str(), str.size());
        Put<char>('\0');
    }

    void Put(std::wstring const &str) {
        PutData((void *)str.c_str(), str.size() * 2);
        Put<wchar_t>(L'\0');
    }

    void Put(void *data, unsigned int size) {
        PutData(data, size);
    }
};
