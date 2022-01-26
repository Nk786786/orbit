// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientData/ModuleAndFunctionLookup.h"

#include <ClientData/CaptureData.h>
#include <ClientData/FunctionUtils.h>
#include <ObjectUtils/Address.h>

using orbit_client_protos::FunctionInfo;
using orbit_client_protos::LinuxAddressInfo;

namespace orbit_client_data {
namespace {
[[nodiscard]] std::optional<uint64_t>
FindFunctionAbsoluteAddressByInstructionAbsoluteAddressUsingModulesInMemory(
    const ProcessData& process, const ModuleManager& module_manager, uint64_t absolute_address) {
  const auto module_or_error = process.FindModuleByAddress(absolute_address);
  if (module_or_error.has_error()) return std::nullopt;
  const auto& module_in_memory = module_or_error.value();
  const uint64_t module_base_address = module_in_memory.start();

  const ModuleData* module = module_manager.GetModuleByModuleInMemoryAndAbsoluteAddress(
      module_in_memory, absolute_address);
  if (module == nullptr) return std::nullopt;

  const uint64_t offset = orbit_object_utils::SymbolAbsoluteAddressToOffset(
      absolute_address, module_base_address, module->executable_segment_offset());
  const auto* function_info = module->FindFunctionByOffset(offset, false);
  if (function_info == nullptr) return std::nullopt;

  return orbit_object_utils::SymbolVirtualAddressToAbsoluteAddress(
      function_info->address(), module_base_address, module->load_bias(),
      module->executable_segment_offset());
}

[[nodiscard]] std::optional<uint64_t>
FindFunctionAbsoluteAddressByInstructionAbsoluteAddressUsingAddressInfo(
    const CaptureData& capture_data, uint64_t absolute_address) {
  const LinuxAddressInfo* address_info = capture_data.GetAddressInfo(absolute_address);
  if (address_info == nullptr) return std::nullopt;

  return absolute_address - address_info->offset_in_function();
}
}  // namespace

const std::string& GetFunctionNameByAddress(const ModuleManager& module_manager,
                                            const CaptureData& capture_data,
                                            uint64_t absolute_address) {
  const FunctionInfo* function = FindFunctionByAddress(*capture_data.process(), module_manager,
                                                       absolute_address, /*is_exact=*/false);
  if (function != nullptr) {
    return orbit_client_data::function_utils::GetDisplayName(*function);
  }
  const LinuxAddressInfo* address_info = capture_data.GetAddressInfo(absolute_address);
  if (address_info == nullptr) {
    return kUnknownFunctionOrModuleName;
  }
  const std::string& function_name = address_info->function_name();
  if (function_name.empty()) {
    return kUnknownFunctionOrModuleName;
  }
  return function_name;
}

// Find the start address of the function this address falls inside. Use the function returned by
// FindFunctionByAddress, and when this fails (e.g., the module containing the function has not
// been loaded) use (for now) the LinuxAddressInfo that is collected for every address in a
// callstack.
std::optional<uint64_t> FindFunctionAbsoluteAddressByInstructionAbsoluteAddress(
    const ModuleManager& module_manager, const CaptureData& capture_data,
    uint64_t absolute_address) {
  auto result = FindFunctionAbsoluteAddressByInstructionAbsoluteAddressUsingModulesInMemory(
      *capture_data.process(), module_manager, absolute_address);
  if (result.has_value()) {
    return result;
  }
  return FindFunctionAbsoluteAddressByInstructionAbsoluteAddressUsingAddressInfo(capture_data,
                                                                                 absolute_address);
}

const FunctionInfo* FindFunctionByModulePathBuildIdAndOffset(const ModuleManager& module_manager,
                                                             const std::string& module_path,
                                                             const std::string& build_id,
                                                             uint64_t offset) {
  const ModuleData* module_data = module_manager.GetModuleByPathAndBuildId(module_path, build_id);
  if (module_data == nullptr) {
    return nullptr;
  }

  uint64_t address = module_data->load_bias() + offset;

  return module_data->FindFunctionByElfAddress(address, /*is_exact=*/true);
}

std::optional<std::string> FindModuleBuildIdByAddress(const ProcessData& process,
                                                      const ModuleManager& module_manager,
                                                      uint64_t absolute_address) {
  const ModuleData* module_data = FindModuleByAddress(process, module_manager, absolute_address);
  if (module_data == nullptr) {
    return std::nullopt;
  }
  return module_data->build_id();
}

const std::string& GetModulePathByAddress(const ModuleManager& module_manager,
                                          const CaptureData& capture_data,
                                          uint64_t absolute_address) {
  const ModuleData* module_data =
      FindModuleByAddress(*capture_data.process(), module_manager, absolute_address);
  if (module_data != nullptr) {
    return module_data->file_path();
  }

  const LinuxAddressInfo* address_info = capture_data.GetAddressInfo(absolute_address);
  if (address_info == nullptr) {
    return kUnknownFunctionOrModuleName;
  }
  const std::string& module_path = address_info->module_path();
  if (module_path.empty()) {
    return kUnknownFunctionOrModuleName;
  }
  return module_path;
}

const FunctionInfo* FindFunctionByAddress(const ProcessData& process,
                                          const ModuleManager& module_manager,
                                          uint64_t absolute_address, bool is_exact) {
  const auto module_or_error = process.FindModuleByAddress(absolute_address);
  if (module_or_error.has_error()) return nullptr;
  const auto& module_in_memory = module_or_error.value();
  const uint64_t module_base_address = module_in_memory.start();

  const ModuleData* module = module_manager.GetModuleByModuleInMemoryAndAbsoluteAddress(
      module_in_memory, absolute_address);
  if (module == nullptr) return nullptr;

  const uint64_t offset = orbit_object_utils::SymbolAbsoluteAddressToOffset(
      absolute_address, module_base_address, module->executable_segment_offset());
  return module->FindFunctionByOffset(offset, is_exact);
}

[[nodiscard]] const ModuleData* FindModuleByAddress(const ProcessData& process,
                                                    const ModuleManager& module_manager,
                                                    uint64_t absolute_address) {
  const auto result = process.FindModuleByAddress(absolute_address);
  if (result.has_error()) return nullptr;
  return module_manager.GetModuleByModuleInMemoryAndAbsoluteAddress(result.value(),
                                                                    absolute_address);
}

std::optional<uint64_t> FindInstrumentedFunctionIdSlow(
    const ModuleManager& module_manager, const CaptureData& capture_data,
    const orbit_client_protos::FunctionInfo& function) {
  const ModuleData* module =
      module_manager.GetModuleByPathAndBuildId(function.module_path(), function.module_build_id());
  for (const auto& it : capture_data.instrumented_functions()) {
    const auto& target_function = it.second;
    if (target_function.file_path() == function.module_path() &&
        target_function.file_offset() ==
            orbit_client_data::function_utils::Offset(function, *module)) {
      return it.first;
    }
  }

  return std::nullopt;
}

}  // namespace orbit_client_data