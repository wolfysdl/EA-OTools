#pragma once
#include <string>
#include <vector>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

enum Rx3SectionId : unsigned int {
    RX3_SECTION_INDEX_BUFFER = 5798132,
    RX3_SECTION_VERTEX_BUFFER = 5798561,
    RX3_SECTION_MATERIAL = 123459928, // ?
    RX3_SECTION_GROUP_NAMES = 230948820, // ?
    RX3_SECTION_SKELETON = 255353250,
    RX3_SECTION_INDEX_BUFFERS_HEADER = 582139446,
    RX3_SECTION_PROP_POSITIONS = 685399266, // ?
    RX3_SECTION_NAMES = 1285267122,
    RX3_SECTION_TEXTURES_HEADER = 1808827868,
    RX3_SECTION_TEXTURE = 2047566042,
    RX3_SECTION_GROUP = 2116321516, // ?
    RX3_SECTION_VERTEX_DECLARATION = 3263271920,
    RX3_SECTION_MESH_RENDER_OPTION = 3566041216, // ???
    RX3_SECTION_BONE_MATRICES = 3751472158,
    RX3_SECTION_COLLISION = 4034198449, // ?
};

class Rx3Section {
    friend class Rx3Container;
    unsigned int id;
public:
    vector<unsigned char> data;

    unsigned int GetID() const;
};

class Rx3Reader {
    unsigned char const *begin;
    unsigned char const *current;
public:
    Rx3Reader(void const *data);
    Rx3Reader(Rx3Section *rx3section);
    Rx3Reader(Rx3Section const *rx3section);
    Rx3Reader(Rx3Section &rx3section);
    Rx3Reader(Rx3Section const &rx3section);
    size_t Position() const;
    void MoveTo(size_t position);
    void Skip(size_t bytes);
    char const *GetString();
    char const *ReadString();
    void const *GetCurrentPtr();

    template<typename T>
    T const &Get() {
        return *(T const *)current;
    }

    template<typename T>
    T const &Read() {
        T const &result = *(T const *)current;
        current += sizeof(T);
        return result;
    }
};

class Rx3Container {
    vector<Rx3Section> sections;
public:
    Rx3Container(path const &rx3path);
    Rx3Section const *FindFirstSection(unsigned int sectionId) const;
    vector<Rx3Section const *> FindAllSections(unsigned int sectionId) const;
};
