#pragma once

#include "DxfTypes.h"

namespace ModaDxf {

RenderPlan CreateRenderPlan(const DxfModel& model);
std::string RenderPlanToJson(const std::string& sourcePath, const DxfModel& model, const RenderPlan& plan);

}  // namespace ModaDxf
