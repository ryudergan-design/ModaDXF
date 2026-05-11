#pragma once

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ModaDxf {

struct DxfPair {
  int code = 0;
  std::string value;
};

struct DxfVertex {
  double x = 0.0;
  double y = 0.0;
  double bulge = 0.0;
};

struct DxfEntity {
  std::string type;
  std::string handle;
  std::string layer = "0";
  std::string blockName;
  std::string text;
  std::vector<DxfPair> rawPairs;
  std::vector<DxfVertex> vertices;
  std::vector<DxfEntity> attribs;
  bool closed = false;
  bool followAttributes = false;
  double x = 0.0;
  double y = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  double radius = 0.0;
  double startAngle = 0.0;
  double endAngle = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double rotation = 0.0;
  double textHeight = 0.0;
};

struct DxfBlock {
  std::string name;
  double baseX = 0.0;
  double baseY = 0.0;
  std::vector<DxfEntity> entities;
};

struct DxfModel {
  std::string acadVersion;
  int unitsCode = 0;
  std::map<std::string, DxfBlock> blocks;
  std::vector<DxfEntity> entities;
  std::set<std::string> layers;
  std::vector<std::string> warnings;
};

struct RenderGroup {
  std::string name;
  std::string blockName;
  std::string handle;
  std::string layer;
  std::map<std::string, int> childEntityCounts;
  int paths = 0;
  int texts = 0;
  int points = 0;
  int nestedGroups = 0;
};

struct RenderStats {
  int blocks = 0;
  int topLevelEntities = 0;
  int topLevelInserts = 0;
  int principalGroups = 0;
  int totalGroups = 0;
  int paths = 0;
  int texts = 0;
  int points = 0;
  int unsupported = 0;
  int layers = 0;
};

struct RenderPlan {
  RenderStats stats;
  std::vector<RenderGroup> principalGroups;
  std::set<std::string> layers;
  std::map<std::string, int> topLevelEntityCounts;
  std::map<std::string, int> blockEntityCounts;
  std::vector<std::string> warnings;
};

}  // namespace ModaDxf
