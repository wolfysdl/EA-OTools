#include "Rx3Container.h"
#include "..\errormsg.h"

unsigned int Rx3Section::GetID() const {
    return id;
}

Rx3Reader::Rx3Reader(void const *data) {
    begin = (unsigned char const *)data;
    current = begin;
}

Rx3Reader::Rx3Reader(Rx3Section *rx3section) : Rx3Reader(rx3section->data.data()) {}
Rx3Reader::Rx3Reader(Rx3Section const *rx3section) : Rx3Reader(rx3section->data.data()) {}
Rx3Reader::Rx3Reader(Rx3Section &rx3section) : Rx3Reader(rx3section.data.data()) {}
Rx3Reader::Rx3Reader(Rx3Section const &rx3section) : Rx3Reader(rx3section.data.data()) {}

size_t Rx3Reader::Position() const {
    return current - begin;
}

void Rx3Reader::MoveTo(size_t position) {
    current = begin + position;
}

void Rx3Reader::Skip(size_t bytes) {
    current += bytes;
}

char const *Rx3Reader::GetString() {
    return (char const *)current;
}

char const *Rx3Reader::ReadString() {
    char const *result = GetString();
    current += strlen(result) + 1;
    return result;
}

void const *Rx3Reader::GetCurrentPtr() {
    return current;
}

Rx3Container::Rx3Container(path const &rx3path) {
    FILE *file = nullptr;
    _wfopen_s(&file, rx3path.c_str(), L"rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        auto fileSize = ftell(file);
        if (fileSize >= 16) {
            fseek(file, 0, SEEK_SET);
            unsigned char *fileData = new unsigned char[fileSize];
            fread(fileData, 1, fileSize, file);
            Rx3Reader reader(fileData);
            if (reader.Read<unsigned int>() == 'l3XR') { // TODO: add big-endian support
                reader.Skip(8);
                unsigned int numSections = reader.Read<unsigned int>();
                if (numSections > 0) {
                    sections.resize(numSections);
                    for (unsigned int s = 0; s < numSections; s++) {
                        unsigned int id = reader.Read<unsigned int>();
                        unsigned int offset = reader.Read<unsigned int>();
                        unsigned int size = reader.Read<unsigned int>();
                        reader.Skip(4);
                        sections[s].id = id;
                        if (size > 0) {
                            sections[s].data.resize(size);
                            memcpy(sections[s].data.data(), &fileData[offset], size);
                        }
                    }
                }
            }
        }
        fclose(file);
    }
}

Rx3Section const *Rx3Container::FindFirstSection(unsigned int sectionId) const {
    for (auto const &s : sections) {
        if (s.GetID() == sectionId)
            return &s;
    }
    return nullptr;
}

vector<Rx3Section const *> Rx3Container::FindAllSections(unsigned int sectionId) const {
    vector<Rx3Section const *> result;
    for (auto const &s : sections) {
        if (s.GetID() == sectionId)
            result.push_back(&s);
    }
    return result;
}
