#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../core/DxfParser.h"
#include "../core/RenderPlan.h"

namespace fs = std::filesystem;

namespace {

void PrintUsage() {
  std::cout
      << "ModaDxfDiagnostic - valida Moldes DXF para importacao agrupada\n\n"
      << "Uso:\n"
      << "  ModaDxfDiagnostic.exe [--json] [--out <pasta>] <molde.dxf>\n";
}

std::string BaseNameForLog(const fs::path& filePath) {
  std::string name = filePath.filename().string();
  for (char& ch : name) {
    if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
        ch == '|') {
      ch = '_';
    }
  }
  return name;
}

void WriteTextFile(const fs::path& filePath, const std::string& content) {
  fs::create_directories(filePath.parent_path());
  std::ofstream output(filePath, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Nao foi possivel escrever log: " + filePath.string());
  }
  output << content;
}

void PrintHumanSummary(const fs::path& source, const ModaDxf::DxfModel& model, const ModaDxf::RenderPlan& plan) {
  std::cout << source.filename().string() << ": "
            << "acad=" << (model.acadVersion.empty() ? "(desconhecido)" : model.acadVersion)
            << " blocks=" << plan.stats.blocks
            << " inserts=" << plan.stats.topLevelInserts
            << " groups=" << plan.stats.principalGroups
            << " paths=" << plan.stats.paths
            << " texts=" << plan.stats.texts
            << " points=" << plan.stats.points
            << " layers=" << plan.stats.layers
            << " unsupported=" << plan.stats.unsupported
            << " match=" << (plan.stats.topLevelInserts == plan.stats.principalGroups ? "yes" : "no")
            << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  bool json = false;
  fs::path outputDir;
  std::vector<fs::path> files;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return 0;
    }
    if (arg == "--json") {
      json = true;
      continue;
    }
    if (arg == "--out") {
      if (i + 1 >= argc) {
        std::cerr << "--out exige uma pasta.\n";
        return 2;
      }
      outputDir = argv[++i];
      continue;
    }
    files.emplace_back(arg);
  }

  if (files.empty()) {
    PrintUsage();
    return 2;
  }

  std::vector<std::string> jsonDocuments;
  int failures = 0;

  for (const auto& file : files) {
    try {
      const ModaDxf::DxfModel model = ModaDxf::ParseDxfFile(file);
      const ModaDxf::RenderPlan plan = ModaDxf::CreateRenderPlan(model);
      const std::string document = ModaDxf::RenderPlanToJson(file.string(), model, plan);

      if (!outputDir.empty()) {
        const fs::path target = outputDir / (BaseNameForLog(file) + ".modadxf-diagnostic.json");
        WriteTextFile(target, document + "\n");
      }

      if (json) {
        jsonDocuments.push_back(document);
      } else {
        PrintHumanSummary(file, model, plan);
      }

      if (plan.stats.topLevelInserts != plan.stats.principalGroups || plan.stats.unsupported != 0) {
        ++failures;
      }
    } catch (const std::exception& ex) {
      ++failures;
      if (json) {
        jsonDocuments.push_back(std::string("{\"source\":\"") + file.string() + "\",\"error\":\"" + ex.what() + "\"}");
      } else {
        std::cerr << file.string() << ": ERRO: " << ex.what() << "\n";
      }
    }
  }

  if (json) {
    std::cout << "[";
    for (size_t i = 0; i < jsonDocuments.size(); ++i) {
      if (i != 0) {
        std::cout << ",";
      }
      std::cout << jsonDocuments[i];
    }
    std::cout << "]\n";
  }

  return failures == 0 ? 0 : 1;
}
