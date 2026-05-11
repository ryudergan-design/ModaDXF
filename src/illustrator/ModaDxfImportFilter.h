#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../core/DxfTypes.h"

namespace ModaDxf {

struct IllustratorPathCommand {
  std::string sourceType;
  std::string layer;
  std::string name;
  std::vector<DxfVertex> vertices;
  bool closed = false;
  double radius = 0.0;
  double startAngle = 0.0;
  double endAngle = 0.0;
};

struct IllustratorTextCommand {
  std::string layer;
  std::string text;
  double x = 0.0;
  double y = 0.0;
  double height = 0.0;
  double rotation = 0.0;
};

struct IllustratorGroupCommand {
  std::string name;
  std::string blockName;
  std::string layer;
  std::vector<IllustratorPathCommand> paths;
  std::vector<IllustratorTextCommand> texts;
  std::vector<DxfVertex> points;
};

struct IllustratorImportPlan {
  std::filesystem::path sourcePath;
  std::vector<IllustratorGroupCommand> principalGroups;
  std::vector<IllustratorTextCommand> looseTexts;
  std::vector<std::string> warnings;
};

IllustratorImportPlan BuildIllustratorImportPlan(const std::filesystem::path& sourcePath, const DxfModel& model);

}  // namespace ModaDxf
