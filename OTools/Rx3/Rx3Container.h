#pragma once
#include <string>
#include <vector>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

enum Rx3SectionId {
    RX3_SECTION_INDEX_BUFFER = 5798132,
    RX3_SECTION_VERTEX_BUFFER = 5798561,
    RX3_SECTION_MATERIAL = 123459928,
    RX3_SECTION_GROUP_NAMES = 230948820,
    RX3_SECTION_INDEX_BUFFERS_HEADER = 582139446,
    RX3_SECTION_PROP_POSITIONS = 685399266,
    RX3_SECTION_NAMES = 1285267122,
    RX3_SECTION_TEXTURES_HEADER = 1808827868,
    RX3_SECTION_TEXTURE = 2047566042,
    RX3_SECTION_GROUP = 2116321516,
    RX3_SECTION_VERTEX_DECLARATION = 3263271920,
    RX3_SECTION_SKELETON = 3751472158,
    RX3_SECTION_COLLISION = 4034198449,
};

class Rx3Section {
    unsigned int id;
public:
    vector<unsigned char> data;

    unsigned int GetID() const {
        return id;
    }
};

class Rx3SectionReader {
    unsigned char const *begin;
    unsigned char const *current;

    Rx3SectionReader(Rx3Section const *rx3section) {
        begin = (unsigned char const *)rx3section;
        current = begin;
    }

    size_t Position() const {
        return current - begin;
    }

    void MoveTo(size_t position) {
        current = begin + position;
    }

    void Skip(size_t bytes) {
        current += bytes;
    }
};

class Rx3Container {
    vector<Rx3Section> sections;

public:
    Rx3Container(path const &rx3path) {

    }

    Rx3Section const *FindFirstSection(unsigned int sectionId) const {
        for (auto const &s : sections) {
            if (s.GetID() == sectionId)
                return &s;
        }
    }

    vector<Rx3Section const *> FindAllSections(unsigned int sectionId) const {
        vector<Rx3Section const *> result;
        for (auto const &s : sections) {
            if (s.GetID() == sectionId)
                result.push_back(&s);
        }
        return result;
    }
};

vector<pair<unsigned int, string>> ExtractNamesFromSection(Rx3Section const *namesSection) {
    unsigned int numNames = *(unsigned int *)(namesSection->data.data() + 4);

    for (unsigned int i = 0; i < numNames; i++) {

    }
}

void ExtractTexturesFromContainer(Rx3Container const &container, path const &outputDir) {
    create_directories(outputDir);
    auto texNamesSection = container.FindFirstSection(RX3_SECTION_NAMES);
    vector<string> texNames;
    if (texNamesSection) {
        // parse texture names
    }
    auto textureSections = container.FindAllSections(RX3_SECTION_TEXTURE);
    for (size_t i = 0; i < textureSections.size(); i++) {

        string texName;
        if (i < texNames.size() && !texNames[i].empty())
            texName = texNames[i];
        else
            texName = "unnamed." + to_string(i);

    }
}
