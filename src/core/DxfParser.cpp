#include "DxfParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ModaDxf {
namespace {

std::string Trim(const std::string& value) {
  auto begin = value.begin();
  while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
    ++begin;
  }

  auto end = value.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
    --end;
  }

  return std::string(begin, end);
}

std::string Upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
}

double ToDouble(const std::string& value, double fallback = 0.0) {
  try {
    size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    return consumed == 0 ? fallback : parsed;
  } catch (...) {
    return fallback;
  }
}

int ToInt(const std::string& value, int fallback = 0) {
  try {
    size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    return consumed == 0 ? fallback : parsed;
  } catch (...) {
    return fallback;
  }
}

std::string FirstStringForCode(const std::vector<DxfPair>& pairs, int code, const std::string& fallback = "") {
  for (const auto& pair : pairs) {
    if (pair.code == code) {
      return pair.value;
    }
  }
  return fallback;
}

double FirstDoubleForCode(const std::vector<DxfPair>& pairs, int code, double fallback = 0.0) {
  for (const auto& pair : pairs) {
    if (pair.code == code) {
      return ToDouble(pair.value, fallback);
    }
  }
  return fallback;
}

int FirstIntForCode(const std::vector<DxfPair>& pairs, int code, int fallback = 0) {
  for (const auto& pair : pairs) {
    if (pair.code == code) {
      return ToInt(pair.value, fallback);
    }
  }
  return fallback;
}

std::vector<DxfPair> ParsePairs(const std::string& text) {
  std::vector<DxfPair> pairs;
  std::istringstream stream(text);
  std::string codeLine;
  std::string valueLine;

  while (std::getline(stream, codeLine)) {
    if (!std::getline(stream, valueLine)) {
      break;
    }

    codeLine = Trim(codeLine);
    if (codeLine.empty()) {
      continue;
    }

    try {
      pairs.push_back({std::stoi(codeLine), Trim(valueLine)});
    } catch (...) {
      // Ignore malformed loose lines. The parser reports semantic issues later.
    }
  }

  return pairs;
}

std::map<std::string, std::vector<DxfPair>> SplitSections(const std::vector<DxfPair>& pairs) {
  std::map<std::string, std::vector<DxfPair>> sections;

  for (size_t i = 0; i < pairs.size(); ++i) {
    if (pairs[i].code == 0 && Upper(pairs[i].value) == "SECTION" && i + 1 < pairs.size() && pairs[i + 1].code == 2) {
      const std::string sectionName = Upper(pairs[i + 1].value);
      i += 2;
      while (i < pairs.size() && !(pairs[i].code == 0 && Upper(pairs[i].value) == "ENDSEC")) {
        sections[sectionName].push_back(pairs[i]);
        ++i;
      }
    }
  }

  return sections;
}

DxfEntity ParseEntityRecord(const std::string& type, const std::vector<DxfPair>& pairs) {
  DxfEntity entity;
  entity.type = Upper(type);
  entity.rawPairs = pairs;
  entity.handle = FirstStringForCode(pairs, 5, "");
  entity.layer = FirstStringForCode(pairs, 8, "0");

  if (entity.type == "LINE") {
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
    entity.x2 = FirstDoubleForCode(pairs, 11);
    entity.y2 = FirstDoubleForCode(pairs, 21);
  } else if (entity.type == "POINT") {
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
  } else if (entity.type == "CIRCLE") {
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
    entity.radius = FirstDoubleForCode(pairs, 40);
  } else if (entity.type == "ARC") {
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
    entity.radius = FirstDoubleForCode(pairs, 40);
    entity.startAngle = FirstDoubleForCode(pairs, 50);
    entity.endAngle = FirstDoubleForCode(pairs, 51);
  } else if (entity.type == "INSERT") {
    entity.blockName = FirstStringForCode(pairs, 2, "");
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
    entity.scaleX = FirstDoubleForCode(pairs, 41, 1.0);
    entity.scaleY = FirstDoubleForCode(pairs, 42, 1.0);
    entity.rotation = FirstDoubleForCode(pairs, 50);
    entity.followAttributes = FirstIntForCode(pairs, 66, 0) == 1;
  } else if (entity.type == "TEXT" || entity.type == "MTEXT" || entity.type == "ATTRIB" || entity.type == "ATTDEF") {
    std::string text = FirstStringForCode(pairs, 1, "");
    for (const auto& pair : pairs) {
      if (entity.type == "MTEXT" && pair.code == 3) {
        text += pair.value;
      }
    }
    entity.text = text;
    entity.x = FirstDoubleForCode(pairs, 10);
    entity.y = FirstDoubleForCode(pairs, 20);
    entity.textHeight = FirstDoubleForCode(pairs, 40, 10.0);
    entity.rotation = FirstDoubleForCode(pairs, 50);
  } else if (entity.type == "LWPOLYLINE" || entity.type == "SPLINE") {
    entity.closed = (FirstIntForCode(pairs, 70, 0) & 1) == 1;
    DxfVertex current;
    bool hasX = false;
    for (const auto& pair : pairs) {
      if (pair.code == 10) {
        if (hasX) {
          entity.vertices.push_back(current);
          current = DxfVertex{};
        }
        current.x = ToDouble(pair.value);
        hasX = true;
      } else if (pair.code == 20 && hasX) {
        current.y = ToDouble(pair.value);
      } else if (pair.code == 42 && hasX) {
        current.bulge = ToDouble(pair.value);
      }
    }
    if (hasX) {
      entity.vertices.push_back(current);
    }
  }

  return entity;
}

struct ParsedEntity {
  DxfEntity entity;
  size_t nextIndex = 0;
};

ParsedEntity ParseInsertEntity(const std::vector<DxfPair>& content, size_t startIndex) {
  std::vector<DxfPair> headerPairs;
  size_t index = startIndex + 1;

  // Important Modaris rule: INSERT records are consecutive and normally do not
  // carry ATTRIB/SEQEND. Stop at the next group-code 0 boundary unless code
  // 66=1 declares attached attributes for this INSERT.
  while (index < content.size() && content[index].code != 0) {
    headerPairs.push_back(content[index]);
    ++index;
  }

  DxfEntity entity = ParseEntityRecord("INSERT", headerPairs);
  if (!entity.followAttributes) {
    return {entity, index};
  }

  while (index < content.size()) {
    if (content[index].code != 0) {
      ++index;
      continue;
    }

    const std::string type = Upper(content[index].value);
    if (type == "SEQEND") {
      ++index;
      while (index < content.size() && content[index].code != 0) {
        ++index;
      }
      break;
    }

    if (type != "ATTRIB") {
      break;
    }

    std::vector<DxfPair> attribPairs;
    ++index;
    while (index < content.size() && content[index].code != 0) {
      attribPairs.push_back(content[index]);
      ++index;
    }
    entity.attribs.push_back(ParseEntityRecord("ATTRIB", attribPairs));
  }

  return {entity, index};
}

ParsedEntity ParsePolylineEntity(const std::vector<DxfPair>& content, size_t startIndex) {
  std::vector<DxfPair> headerPairs;
  size_t index = startIndex + 1;
  while (index < content.size() && content[index].code != 0) {
    headerPairs.push_back(content[index]);
    ++index;
  }

  DxfEntity entity = ParseEntityRecord("POLYLINE", headerPairs);
  entity.closed = (FirstIntForCode(headerPairs, 70, 0) & 1) == 1;

  while (index < content.size()) {
    if (content[index].code != 0) {
      ++index;
      continue;
    }

    const std::string type = Upper(content[index].value);
    if (type == "SEQEND") {
      ++index;
      while (index < content.size() && content[index].code != 0) {
        ++index;
      }
      break;
    }

    if (type != "VERTEX") {
      break;
    }

    std::vector<DxfPair> vertexPairs;
    ++index;
    while (index < content.size() && content[index].code != 0) {
      vertexPairs.push_back(content[index]);
      ++index;
    }

    entity.vertices.push_back({
        FirstDoubleForCode(vertexPairs, 10),
        FirstDoubleForCode(vertexPairs, 20),
        FirstDoubleForCode(vertexPairs, 42),
    });
  }

  return {entity, index};
}

ParsedEntity ParseGenericEntity(const std::vector<DxfPair>& content, size_t startIndex) {
  const std::string type = Upper(content[startIndex].value);
  std::vector<DxfPair> pairs;
  size_t index = startIndex + 1;
  while (index < content.size() && content[index].code != 0) {
    pairs.push_back(content[index]);
    ++index;
  }
  return {ParseEntityRecord(type, pairs), index};
}

ParsedEntity ParseEntityAt(const std::vector<DxfPair>& content, size_t startIndex) {
  const std::string type = Upper(content[startIndex].value);
  if (type == "INSERT") {
    return ParseInsertEntity(content, startIndex);
  }
  if (type == "POLYLINE") {
    return ParsePolylineEntity(content, startIndex);
  }
  return ParseGenericEntity(content, startIndex);
}

void RegisterLayer(DxfModel& model, const DxfEntity& entity) {
  if (!entity.layer.empty()) {
    model.layers.insert(entity.layer);
  }
  for (const auto& attrib : entity.attribs) {
    if (!attrib.layer.empty()) {
      model.layers.insert(attrib.layer);
    }
  }
}

void ParseHeaderSection(const std::vector<DxfPair>& content, DxfModel& model) {
  for (size_t i = 0; i + 1 < content.size(); ++i) {
    if (content[i].code == 9 && Upper(content[i].value) == "$ACADVER") {
      model.acadVersion = content[i + 1].value;
    } else if (content[i].code == 9 && Upper(content[i].value) == "$INSUNITS") {
      model.unitsCode = ToInt(content[i + 1].value, 0);
    }
  }
}

void ParseTablesSection(const std::vector<DxfPair>& content, DxfModel& model) {
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i].code == 0 && Upper(content[i].value) == "LAYER") {
      ++i;
      while (i < content.size() && content[i].code != 0) {
        if (content[i].code == 2 && !content[i].value.empty()) {
          model.layers.insert(content[i].value);
        }
        ++i;
      }
      if (i > 0) {
        --i;
      }
    }
  }
}

void ParseBlocksSection(const std::vector<DxfPair>& content, DxfModel& model) {
  for (size_t i = 0; i < content.size();) {
    if (!(content[i].code == 0 && Upper(content[i].value) == "BLOCK")) {
      ++i;
      continue;
    }

    std::vector<DxfPair> headerPairs;
    ++i;
    while (i < content.size() && content[i].code != 0) {
      headerPairs.push_back(content[i]);
      ++i;
    }

    DxfBlock block;
    block.name = FirstStringForCode(headerPairs, 2, FirstStringForCode(headerPairs, 3, ""));
    block.baseX = FirstDoubleForCode(headerPairs, 10);
    block.baseY = FirstDoubleForCode(headerPairs, 20);

    while (i < content.size()) {
      if (content[i].code == 0 && Upper(content[i].value) == "ENDBLK") {
        ++i;
        while (i < content.size() && content[i].code != 0) {
          ++i;
        }
        break;
      }

      if (content[i].code != 0) {
        ++i;
        continue;
      }

      const std::string type = Upper(content[i].value);
      if (type == "VERTEX" || type == "SEQEND") {
        ++i;
        continue;
      }

      ParsedEntity parsed = ParseEntityAt(content, i);
      RegisterLayer(model, parsed.entity);
      block.entities.push_back(parsed.entity);
      i = parsed.nextIndex;
    }

    if (!block.name.empty()) {
      model.blocks[block.name] = block;
    } else {
      model.warnings.push_back("Bloco sem nome ignorado.");
    }
  }
}

void ParseEntitiesSection(const std::vector<DxfPair>& content, DxfModel& model) {
  for (size_t i = 0; i < content.size();) {
    if (content[i].code != 0) {
      ++i;
      continue;
    }

    const std::string type = Upper(content[i].value);
    if (type == "VERTEX" || type == "SEQEND") {
      ++i;
      continue;
    }

    ParsedEntity parsed = ParseEntityAt(content, i);
    RegisterLayer(model, parsed.entity);
    model.entities.push_back(parsed.entity);
    i = parsed.nextIndex;
  }
}

}  // namespace

DxfModel ParseDxfText(const std::string& text) {
  DxfModel model;
  const auto pairs = ParsePairs(text);
  const auto sections = SplitSections(pairs);

  if (const auto it = sections.find("HEADER"); it != sections.end()) {
    ParseHeaderSection(it->second, model);
  }
  if (const auto it = sections.find("TABLES"); it != sections.end()) {
    ParseTablesSection(it->second, model);
  }
  if (const auto it = sections.find("BLOCKS"); it != sections.end()) {
    ParseBlocksSection(it->second, model);
  }
  if (const auto it = sections.find("ENTITIES"); it != sections.end()) {
    ParseEntitiesSection(it->second, model);
  }

  if (model.acadVersion.empty()) {
    model.warnings.push_back("HEADER sem $ACADVER.");
  }
  if (model.blocks.empty()) {
    model.warnings.push_back("DXF sem blocos em BLOCKS.");
  }

  return model;
}

DxfModel ParseDxfFile(const std::filesystem::path& filePath) {
  std::ifstream input(filePath, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Nao foi possivel abrir DXF: " + filePath.string());
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return ParseDxfText(buffer.str());
}

}  // namespace ModaDxf
