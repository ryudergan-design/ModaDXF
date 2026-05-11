#include "ModaDxfImportFilter.h"

#include <cmath>

namespace ModaDxf {
namespace {

struct AffineTransform {
  double a = 1.0;
  double b = 0.0;
  double c = 0.0;
  double d = 1.0;
  double tx = 0.0;
  double ty = 0.0;
};

constexpr double Pi = 3.14159265358979323846;

AffineTransform Compose(const AffineTransform& left, const AffineTransform& right) {
  return {
      left.a * right.a + left.c * right.b,
      left.b * right.a + left.d * right.b,
      left.a * right.c + left.c * right.d,
      left.b * right.c + left.d * right.d,
      left.a * right.tx + left.c * right.ty + left.tx,
      left.b * right.tx + left.d * right.ty + left.ty,
  };
}

AffineTransform Translation(double x, double y) {
  return {1.0, 0.0, 0.0, 1.0, x, y};
}

AffineTransform Scale(double x, double y) {
  return {x, 0.0, 0.0, y, 0.0, 0.0};
}

AffineTransform Rotation(double degrees) {
  const double radians = degrees * Pi / 180.0;
  const double s = std::sin(radians);
  const double c = std::cos(radians);
  return {c, s, -s, c, 0.0, 0.0};
}

DxfVertex Apply(const AffineTransform& transform, double x, double y, double bulge = 0.0) {
  return {
      transform.a * x + transform.c * y + transform.tx,
      transform.b * x + transform.d * y + transform.ty,
      bulge,
  };
}

AffineTransform InsertTransform(const DxfEntity& insert, const DxfBlock& block) {
  AffineTransform transform;
  transform = Compose(transform, Translation(insert.x, insert.y));
  transform = Compose(transform, Rotation(insert.rotation));
  transform = Compose(transform, Scale(insert.scaleX, insert.scaleY));
  transform = Compose(transform, Translation(-block.baseX, -block.baseY));
  return transform;
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

bool IsTextEntity(const DxfEntity& entity) {
  return entity.type == "TEXT" || entity.type == "MTEXT" || entity.type == "ATTRIB" || entity.type == "ATTDEF";
}

bool IsPathEntity(const DxfEntity& entity) {
  return entity.type == "LINE" || entity.type == "POLYLINE" || entity.type == "LWPOLYLINE" || entity.type == "ARC" ||
         entity.type == "CIRCLE" || entity.type == "SPLINE";
}

IllustratorTextCommand ToTextCommand(const DxfEntity& entity, const std::string& layer, const AffineTransform& transform) {
  const DxfVertex point = Apply(transform, entity.x, entity.y);
  return {
      layer,
      entity.text,
      point.x,
      point.y,
      entity.textHeight,
      entity.rotation,
  };
}

IllustratorPathCommand ToPathCommand(const DxfEntity& entity, const std::string& layer, const AffineTransform& transform) {
  IllustratorPathCommand command;
  command.sourceType = entity.type;
  command.layer = layer;
  command.name = entity.type;
  command.closed = entity.closed || entity.type == "CIRCLE";
  command.radius = entity.radius;
  command.startAngle = entity.startAngle;
  command.endAngle = entity.endAngle;

  if (entity.type == "LINE") {
    command.vertices.push_back(Apply(transform, entity.x, entity.y));
    command.vertices.push_back(Apply(transform, entity.x2, entity.y2));
  } else if (entity.type == "CIRCLE" || entity.type == "ARC") {
    double start = entity.type == "CIRCLE" ? 0.0 : entity.startAngle;
    double end = entity.type == "CIRCLE" ? 360.0 : entity.endAngle;
    if (entity.type == "ARC" && end < start) {
      end += 360.0;
    }
    const double sweep = std::max(1.0, std::abs(end - start));
    const int steps = std::max(8, std::min(96, static_cast<int>(std::ceil(sweep / 6.0))));
    for (int i = 0; i <= steps; ++i) {
      const double angle = (start + (end - start) * (static_cast<double>(i) / steps)) * Pi / 180.0;
      command.vertices.push_back(Apply(transform,
                                       entity.x + std::cos(angle) * entity.radius,
                                       entity.y + std::sin(angle) * entity.radius));
    }
  } else {
    for (const auto& vertex : entity.vertices) {
      command.vertices.push_back(Apply(transform, vertex.x, vertex.y, vertex.bulge));
    }
  }

  return command;
}

void AddEntityToGroup(const DxfEntity& entity,
                      const DxfModel& model,
                      IllustratorGroupCommand& group,
                      IllustratorImportPlan& plan,
                      const std::string& inheritedLayer,
                      const AffineTransform& transform,
                      int depth) {
  const std::string layer = EffectiveLayer(entity, inheritedLayer);

  if (IsPathEntity(entity)) {
    group.paths.push_back(ToPathCommand(entity, layer, transform));
    return;
  }

  if (IsTextEntity(entity)) {
    group.texts.push_back(ToTextCommand(entity, layer, transform));
    return;
  }

  if (entity.type == "POINT") {
    group.points.push_back(Apply(transform, entity.x, entity.y));
    return;
  }

  if (entity.type == "INSERT") {
    if (depth > 16) {
      plan.warnings.push_back("Profundidade maxima de INSERT atingida em " + entity.blockName);
      return;
    }

    const auto blockIt = model.blocks.find(entity.blockName);
    if (blockIt == model.blocks.end()) {
      plan.warnings.push_back("Bloco nao encontrado para INSERT aninhado: " + entity.blockName);
      return;
    }

    const AffineTransform nestedTransform = Compose(transform, InsertTransform(entity, blockIt->second));
    for (const auto& child : blockIt->second.entities) {
      AddEntityToGroup(child, model, group, plan, layer, nestedTransform, depth + 1);
    }
    for (const auto& attrib : entity.attribs) {
      AddEntityToGroup(attrib, model, group, plan, layer, nestedTransform, depth + 1);
    }
    return;
  }

  plan.warnings.push_back("Entidade ainda nao convertida para comando Illustrator: " + entity.type);
}

}  // namespace

IllustratorImportPlan BuildIllustratorImportPlan(const std::filesystem::path& sourcePath, const DxfModel& model) {
  IllustratorImportPlan plan;
  plan.sourcePath = sourcePath;
  plan.warnings = model.warnings;

  for (const auto& entity : model.entities) {
    const std::string layer = EffectiveLayer(entity, "");

    if (entity.type == "INSERT") {
      IllustratorGroupCommand group;
      group.name = "INSERT " + entity.blockName;
      group.blockName = entity.blockName;
      group.layer = layer;

      const auto blockIt = model.blocks.find(entity.blockName);
      if (blockIt == model.blocks.end()) {
        plan.warnings.push_back("Bloco nao encontrado para INSERT principal: " + entity.blockName);
      } else {
        const AffineTransform transform = InsertTransform(entity, blockIt->second);
        for (const auto& child : blockIt->second.entities) {
          AddEntityToGroup(child, model, group, plan, layer, transform, 1);
        }
        for (const auto& attrib : entity.attribs) {
          AddEntityToGroup(attrib, model, group, plan, layer, transform, 1);
        }
      }

      plan.principalGroups.push_back(group);
      continue;
    }

    if (IsTextEntity(entity)) {
      plan.looseTexts.push_back(ToTextCommand(entity, layer, AffineTransform{}));
    }
  }

  return plan;
}

}  // namespace ModaDxf
