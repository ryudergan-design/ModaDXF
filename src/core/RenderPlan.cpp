#include "RenderPlan.h"

#include <sstream>

namespace ModaDxf {
namespace {

bool IsPathEntity(const std::string& type) {
  return type == "LINE" || type == "POLYLINE" || type == "LWPOLYLINE" || type == "ARC" || type == "CIRCLE" ||
         type == "SPLINE";
}

bool IsTextEntity(const std::string& type) {
  return type == "TEXT" || type == "MTEXT" || type == "ATTRIB" || type == "ATTDEF";
}

std::string EffectiveLayer(const DxfEntity& entity, const std::string& inheritedLayer) {
  if (!entity.layer.empty() && entity.layer != "0") {
    return entity.layer;
  }
  if (!inheritedLayer.empty()) {
    return inheritedLayer;
  }
  return "0";
}

void AddCount(std::map<std::string, int>& counts, const std::string& key, int by = 1) {
  counts[key] += by;
}

void CountBlockEntities(const DxfBlock& block, std::map<std::string, int>& counts) {
  for (const auto& entity : block.entities) {
    AddCount(counts, entity.type);
  }
}

void RenderEntity(const DxfEntity& entity,
                  const DxfModel& model,
                  RenderPlan& plan,
                  const std::string& inheritedLayer,
                  RenderGroup* ownerGroup,
                  int depth) {
  const std::string layer = EffectiveLayer(entity, inheritedLayer);
  plan.layers.insert(layer);

  if (ownerGroup != nullptr) {
    AddCount(ownerGroup->childEntityCounts, entity.type);
  }

  if (IsPathEntity(entity.type)) {
    ++plan.stats.paths;
    if (ownerGroup != nullptr) {
      ++ownerGroup->paths;
    }
    return;
  }

  if (entity.type == "POINT") {
    ++plan.stats.points;
    if (ownerGroup != nullptr) {
      ++ownerGroup->points;
    }
    return;
  }

  if (IsTextEntity(entity.type)) {
    ++plan.stats.texts;
    if (ownerGroup != nullptr) {
      ++ownerGroup->texts;
    }
    return;
  }

  if (entity.type == "INSERT") {
    const auto blockIt = model.blocks.find(entity.blockName);
    if (blockIt == model.blocks.end()) {
      plan.warnings.push_back("Bloco nao encontrado para INSERT: " + entity.blockName);
      ++plan.stats.unsupported;
      return;
    }

    if (depth > 16) {
      plan.warnings.push_back("Profundidade maxima de INSERT atingida em " + entity.blockName);
      ++plan.stats.unsupported;
      return;
    }

    ++plan.stats.totalGroups;
    if (ownerGroup != nullptr) {
      ++ownerGroup->nestedGroups;
    }

    RenderGroup nested;
    nested.name = "INSERT " + entity.blockName;
    nested.blockName = entity.blockName;
    nested.handle = entity.handle;
    nested.layer = layer;

    for (const auto& child : blockIt->second.entities) {
      RenderEntity(child, model, plan, layer, &nested, depth + 1);
    }
    for (const auto& attrib : entity.attribs) {
      RenderEntity(attrib, model, plan, layer, &nested, depth + 1);
    }
    return;
  }

  plan.warnings.push_back("Entidade nao suportada no plano: " + entity.type);
  ++plan.stats.unsupported;
}

std::string JsonEscape(const std::string& value) {
  std::ostringstream out;
  for (const char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out << "\\u00";
          const char* hex = "0123456789abcdef";
          out << hex[(ch >> 4) & 0xF] << hex[ch & 0xF];
        } else {
          out << ch;
        }
    }
  }
  return out.str();
}

void WriteStringArray(std::ostringstream& out, const std::vector<std::string>& values) {
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ",";
    }
    out << "\"" << JsonEscape(values[i]) << "\"";
  }
  out << "]";
}

void WriteSetArray(std::ostringstream& out, const std::set<std::string>& values) {
  out << "[";
  size_t index = 0;
  for (const auto& value : values) {
    if (index++ != 0) {
      out << ",";
    }
    out << "\"" << JsonEscape(value) << "\"";
  }
  out << "]";
}

void WriteCounts(std::ostringstream& out, const std::map<std::string, int>& counts) {
  out << "{";
  size_t index = 0;
  for (const auto& [key, value] : counts) {
    if (index++ != 0) {
      out << ",";
    }
    out << "\"" << JsonEscape(key) << "\":" << value;
  }
  out << "}";
}

}  // namespace

RenderPlan CreateRenderPlan(const DxfModel& model) {
  RenderPlan plan;
  plan.stats.blocks = static_cast<int>(model.blocks.size());
  plan.stats.topLevelEntities = static_cast<int>(model.entities.size());
  plan.warnings = model.warnings;

  for (const auto& [name, block] : model.blocks) {
    (void)name;
    CountBlockEntities(block, plan.blockEntityCounts);
  }

  for (const auto& entity : model.entities) {
    AddCount(plan.topLevelEntityCounts, entity.type);
    if (entity.type == "INSERT") {
      ++plan.stats.topLevelInserts;
      ++plan.stats.principalGroups;
      ++plan.stats.totalGroups;

      const std::string layer = EffectiveLayer(entity, "");
      RenderGroup group;
      group.name = "INSERT " + entity.blockName;
      group.blockName = entity.blockName;
      group.handle = entity.handle;
      group.layer = layer;
      plan.layers.insert(layer);

      const auto blockIt = model.blocks.find(entity.blockName);
      if (blockIt == model.blocks.end()) {
        plan.warnings.push_back("Bloco nao encontrado para INSERT principal: " + entity.blockName);
        ++plan.stats.unsupported;
      } else {
        for (const auto& child : blockIt->second.entities) {
          RenderEntity(child, model, plan, layer, &group, 1);
        }
        for (const auto& attrib : entity.attribs) {
          RenderEntity(attrib, model, plan, layer, &group, 1);
        }
      }

      plan.principalGroups.push_back(group);
    } else {
      RenderEntity(entity, model, plan, "", nullptr, 0);
    }
  }

  for (const auto& layer : model.layers) {
    if (!layer.empty()) {
      plan.layers.insert(layer);
    }
  }
  plan.stats.layers = static_cast<int>(plan.layers.size());

  return plan;
}

std::string RenderPlanToJson(const std::string& sourcePath, const DxfModel& model, const RenderPlan& plan) {
  std::ostringstream out;
  out << "{";
  out << "\"source\":\"" << JsonEscape(sourcePath) << "\",";
  out << "\"summary\":\"ModaDXF leu o Molde DXF, reconstruiu os grupos principais e preparou o resultado para o Illustrator.\",";
  out << "\"whatWasDone\":[";
  out << "\"Leitura do Molde DXF concluida.\",";
  out << "\"Agrupamentos principais reconstruidos.\",";
  out << "\"Camadas, caminhos, textos e pontos contabilizados.\",";
  out << "\"Avisos registrados quando encontrados.\"";
  out << "],";
  out << "\"acadVersion\":\"" << JsonEscape(model.acadVersion) << "\",";
  out << "\"unitsCode\":" << model.unitsCode << ",";
  out << "\"stats\":{";
  out << "\"blocks\":" << plan.stats.blocks << ",";
  out << "\"topLevelEntities\":" << plan.stats.topLevelEntities << ",";
  out << "\"topLevelInserts\":" << plan.stats.topLevelInserts << ",";
  out << "\"principalGroups\":" << plan.stats.principalGroups << ",";
  out << "\"totalGroups\":" << plan.stats.totalGroups << ",";
  out << "\"paths\":" << plan.stats.paths << ",";
  out << "\"texts\":" << plan.stats.texts << ",";
  out << "\"points\":" << plan.stats.points << ",";
  out << "\"layers\":" << plan.stats.layers << ",";
  out << "\"unsupported\":" << plan.stats.unsupported;
  out << "},";
  out << "\"acceptance\":{";
  out << "\"insertGroupsMatch\":" << (plan.stats.topLevelInserts == plan.stats.principalGroups ? "true" : "false") << ",";
  out << "\"noMissingBlocks\":" << (plan.stats.unsupported == 0 ? "true" : "false");
  out << "},";
  out << "\"topLevelEntityCounts\":";
  WriteCounts(out, plan.topLevelEntityCounts);
  out << ",\"blockEntityCounts\":";
  WriteCounts(out, plan.blockEntityCounts);
  out << ",\"layers\":";
  WriteSetArray(out, plan.layers);
  out << ",\"principalGroups\":[";
  for (size_t i = 0; i < plan.principalGroups.size(); ++i) {
    if (i != 0) {
      out << ",";
    }
    const auto& group = plan.principalGroups[i];
    out << "{";
    out << "\"name\":\"" << JsonEscape(group.name) << "\",";
    out << "\"blockName\":\"" << JsonEscape(group.blockName) << "\",";
    out << "\"handle\":\"" << JsonEscape(group.handle) << "\",";
    out << "\"layer\":\"" << JsonEscape(group.layer) << "\",";
    out << "\"paths\":" << group.paths << ",";
    out << "\"texts\":" << group.texts << ",";
    out << "\"points\":" << group.points << ",";
    out << "\"nestedGroups\":" << group.nestedGroups << ",";
    out << "\"childEntityCounts\":";
    WriteCounts(out, group.childEntityCounts);
    out << "}";
  }
  out << "],";
  out << "\"warnings\":";
  WriteStringArray(out, plan.warnings);
  out << "}";
  return out.str();
}

}  // namespace ModaDxf
