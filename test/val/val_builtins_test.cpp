// Copyright (c) 2018 Google LLC.
// Modifications Copyright (C) 2020 Advanced Micro Devices, Inc. All rights
// reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests validation rules of GLSL.450.std and OpenCL.std extended instructions.
// Doesn't test OpenCL.std vector size 2, 3, 4, 8 or 16 rules (not supported
// by standard SPIR-V).

#include <cstring>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "test/val/val_code_generator.h"
#include "test/val/val_fixtures.h"

namespace spvtools {
namespace val {
namespace {

struct TestResult {
  TestResult(spv_result_t in_validation_result = SPV_SUCCESS,
             const char* in_error_str = nullptr,
             const char* in_error_str2 = nullptr)
      : validation_result(in_validation_result),
        error_str(in_error_str),
        error_str2(in_error_str2) {}
  spv_result_t validation_result;
  const char* error_str;
  const char* error_str2;
};

using ::testing::Combine;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Values;
using ::testing::ValuesIn;

using ValidateBuiltIns = spvtest::ValidateBase<bool>;
using ValidateVulkanSubgroupBuiltIns =
    spvtest::ValidateBase<std::tuple<const char*, const char*, const char*,
                                     const char*, const char*, TestResult>>;
using ValidateVulkanCombineBuiltInExecutionModelDataTypeResult =
    spvtest::ValidateBase<std::tuple<const char*, const char*, const char*,
                                     const char*, const char*, TestResult>>;
using ValidateVulkanCombineBuiltInArrayedVariable =
    spvtest::ValidateBase<std::tuple<const char*, const char*, const char*,
                                     const char*, const char*, TestResult>>;
using ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult =
    spvtest::ValidateBase<
        std::tuple<const char*, const char*, const char*, const char*,
                   const char*, const char*, const char*, TestResult>>;

using ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult =
    spvtest::ValidateBase<std::tuple<spv_target_env, const char*, const char*,
                                     const char*, const char*, const char*,
                                     const char*, const char*, TestResult>>;

bool InitializerRequired(const char* const storage_class) {
  return (strncmp(storage_class, "Output", 6) == 0 ||
          strncmp(storage_class, "Private", 7) == 0 ||
          strncmp(storage_class, "Function", 8) == 0);
}

CodeGenerator GetInMainCodeGenerator(const char* const built_in,
                                     const char* const execution_model,
                                     const char* const storage_class,
                                     const char* const capabilities,
                                     const char* const extensions,
                                     const char* const data_type) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  if (capabilities) {
    generator.capabilities_ += capabilities;
  }
  if (extensions) {
    generator.extensions_ += extensions;
  }

  generator.before_types_ = R"(OpDecorate %built_in_type Block
                               OpMemberDecorate %built_in_type 0 BuiltIn )";
  generator.before_types_ += built_in;
  generator.before_types_ += "\n";

  if (strncmp(built_in, "TessLevel", 9) == 0) {
    generator.before_types_ += "OpMemberDecorate %built_in_type 0 Patch\n";
  }

  std::ostringstream after_types;

  after_types << "%built_in_type = OpTypeStruct " << data_type << "\n";
  if (InitializerRequired(storage_class)) {
    after_types << "%built_in_null = OpConstantNull %built_in_type\n";
  }
  after_types << "%built_in_ptr = OpTypePointer " << storage_class
              << " %built_in_type\n";
  after_types << "%built_in_var = OpVariable %built_in_ptr " << storage_class;
  if (InitializerRequired(storage_class)) {
    after_types << " %built_in_null";
  }
  after_types << "\n";
  after_types << "%data_ptr = OpTypePointer " << storage_class << " "
              << data_type << "\n";
  generator.after_types_ = after_types.str();

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = execution_model;
  if (strncmp(storage_class, "Input", 5) == 0 ||
      strncmp(storage_class, "Output", 6) == 0) {
    entry_point.interfaces = "%built_in_var";
  }

  std::ostringstream execution_modes;
  if (0 == std::strcmp(execution_model, "Fragment")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OriginUpperLeft\n";
    if (0 == std::strcmp(built_in, "FragDepth")) {
      execution_modes << "OpExecutionMode %" << entry_point.name
                      << " DepthReplacing\n";
    }
  }
  if (0 == std::strcmp(execution_model, "Geometry")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " InputPoints\n";
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OutputPoints\n";
  }
  if (0 == std::strcmp(execution_model, "GLCompute")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " LocalSize 1 1 1\n";
  }
  entry_point.execution_modes = execution_modes.str();

  entry_point.body = R"(
%ptr = OpAccessChain %data_ptr %built_in_var %u32_0
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_P(ValidateVulkanCombineBuiltInExecutionModelDataTypeResult, InMain) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const vuid = std::get<4>(GetParam());
  const TestResult& test_result = std::get<5>(GetParam());

  CodeGenerator generator = GetInMainCodeGenerator(
      built_in, execution_model, storage_class, NULL, NULL, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

TEST_P(
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    InMain) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const capabilities = std::get<4>(GetParam());
  const char* const extensions = std::get<5>(GetParam());
  const char* const vuid = std::get<6>(GetParam());
  const TestResult& test_result = std::get<7>(GetParam());

  CodeGenerator generator =
      GetInMainCodeGenerator(built_in, execution_model, storage_class,
                             capabilities, extensions, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

TEST_P(
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    InMain) {
  const spv_target_env env = std::get<0>(GetParam());
  const char* const built_in = std::get<1>(GetParam());
  const char* const execution_model = std::get<2>(GetParam());
  const char* const storage_class = std::get<3>(GetParam());
  const char* const data_type = std::get<4>(GetParam());
  const char* const capabilities = std::get<5>(GetParam());
  const char* const extensions = std::get<6>(GetParam());
  const char* const vuid = std::get<7>(GetParam());
  const TestResult& test_result = std::get<8>(GetParam());

  CodeGenerator generator =
      GetInMainCodeGenerator(built_in, execution_model, storage_class,
                             capabilities, extensions, data_type);

  CompileSuccessfully(generator.Build(), env);
  ASSERT_EQ(test_result.validation_result, ValidateInstructions(env));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

CodeGenerator GetInFunctionCodeGenerator(const char* const built_in,
                                         const char* const execution_model,
                                         const char* const storage_class,
                                         const char* const capabilities,
                                         const char* const extensions,
                                         const char* const data_type) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  if (capabilities) {
    generator.capabilities_ += capabilities;
  }
  if (extensions) {
    generator.extensions_ += extensions;
  }

  generator.before_types_ = R"(OpDecorate %built_in_type Block
                              OpMemberDecorate %built_in_type 0 BuiltIn )";
  generator.before_types_ += built_in;
  generator.before_types_ += "\n";

  if (strncmp(built_in, "TessLevel", 9) == 0) {
    generator.before_types_ += "OpMemberDecorate %built_in_type 0 Patch\n";
  }

  std::ostringstream after_types;
  after_types << "%built_in_type = OpTypeStruct " << data_type << "\n";
  if (InitializerRequired(storage_class)) {
    after_types << "%built_in_null = OpConstantNull %built_in_type\n";
  }
  after_types << "%built_in_ptr = OpTypePointer " << storage_class
              << " %built_in_type\n";
  after_types << "%built_in_var = OpVariable %built_in_ptr " << storage_class;
  if (InitializerRequired(storage_class)) {
    after_types << " %built_in_null";
  }
  after_types << "\n";
  after_types << "%data_ptr = OpTypePointer " << storage_class << " "
              << data_type << "\n";
  generator.after_types_ = after_types.str();

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = execution_model;
  if (strncmp(storage_class, "Input", 5) == 0 ||
      strncmp(storage_class, "Output", 6) == 0) {
    entry_point.interfaces = "%built_in_var";
  }

  std::ostringstream execution_modes;
  if (0 == std::strcmp(execution_model, "Fragment")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OriginUpperLeft\n";
    if (0 == std::strcmp(built_in, "FragDepth")) {
      execution_modes << "OpExecutionMode %" << entry_point.name
                      << " DepthReplacing\n";
    }
  }
  if (0 == std::strcmp(execution_model, "Geometry")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " InputPoints\n";
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OutputPoints\n";
  }
  if (0 == std::strcmp(execution_model, "GLCompute")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " LocalSize 1 1 1\n";
  }
  entry_point.execution_modes = execution_modes.str();

  entry_point.body = R"(
%val2 = OpFunctionCall %void %foo
)";

  std::string function_body = R"(
%foo = OpFunction %void None %func
%foo_entry = OpLabel
%ptr = OpAccessChain %data_ptr %built_in_var %u32_0
OpReturn
OpFunctionEnd
)";

  generator.add_at_the_end_ = function_body;

  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_P(ValidateVulkanCombineBuiltInExecutionModelDataTypeResult, InFunction) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const vuid = std::get<4>(GetParam());
  const TestResult& test_result = std::get<5>(GetParam());

  CodeGenerator generator = GetInFunctionCodeGenerator(
      built_in, execution_model, storage_class, NULL, NULL, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

TEST_P(
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    InFunction) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const capabilities = std::get<4>(GetParam());
  const char* const extensions = std::get<5>(GetParam());
  const char* const vuid = std::get<6>(GetParam());
  const TestResult& test_result = std::get<7>(GetParam());

  CodeGenerator generator =
      GetInFunctionCodeGenerator(built_in, execution_model, storage_class,
                                 capabilities, extensions, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

CodeGenerator GetVariableCodeGenerator(const char* const built_in,
                                       const char* const execution_model,
                                       const char* const storage_class,
                                       const char* const capabilities,
                                       const char* const extensions,
                                       const char* const data_type) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  if (capabilities) {
    generator.capabilities_ += capabilities;
  }
  if (extensions) {
    generator.extensions_ += extensions;
  }

  generator.before_types_ = "OpDecorate %built_in_var BuiltIn ";
  generator.before_types_ += built_in;
  generator.before_types_ += "\n";

  if (strncmp(built_in, "TessLevel", 9) == 0) {
    generator.before_types_ += "OpDecorate %built_in_var Patch\n";
  }

  if ((0 == std::strcmp(storage_class, "Input")) &&
      (0 == std::strcmp(execution_model, "Fragment"))) {
    // ensure any needed input types that might require Flat
    generator.before_types_ += "OpDecorate %built_in_var Flat\n";
  }

  std::ostringstream after_types;
  if (InitializerRequired(storage_class)) {
    after_types << "%built_in_null = OpConstantNull " << data_type << "\n";
  }
  after_types << "%built_in_ptr = OpTypePointer " << storage_class << " "
              << data_type << "\n";
  after_types << "%built_in_var = OpVariable %built_in_ptr " << storage_class;
  if (InitializerRequired(storage_class)) {
    after_types << " %built_in_null";
  }
  after_types << "\n";
  generator.after_types_ = after_types.str();

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = execution_model;
  if (strncmp(storage_class, "Input", 5) == 0 ||
      strncmp(storage_class, "Output", 6) == 0) {
    entry_point.interfaces = "%built_in_var";
  }
  // Any kind of reference would do.
  entry_point.body = R"(
%val = OpBitcast %u32 %built_in_var
)";

  std::ostringstream execution_modes;
  if (0 == std::strcmp(execution_model, "Fragment")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OriginUpperLeft\n";
    if (0 == std::strcmp(built_in, "FragDepth")) {
      execution_modes << "OpExecutionMode %" << entry_point.name
                      << " DepthReplacing\n";
    }
  }
  if (0 == std::strcmp(execution_model, "Geometry")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " InputPoints\n";
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OutputPoints\n";
  }
  if (0 == std::strcmp(execution_model, "GLCompute")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " LocalSize 1 1 1\n";
  }
  entry_point.execution_modes = execution_modes.str();

  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_P(ValidateVulkanCombineBuiltInExecutionModelDataTypeResult, Variable) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const vuid = std::get<4>(GetParam());
  const TestResult& test_result = std::get<5>(GetParam());

  CodeGenerator generator = GetVariableCodeGenerator(
      built_in, execution_model, storage_class, NULL, NULL, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

TEST_P(
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Variable) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const capabilities = std::get<4>(GetParam());
  const char* const extensions = std::get<5>(GetParam());
  const char* const vuid = std::get<6>(GetParam());
  const TestResult& test_result = std::get<7>(GetParam());

  CodeGenerator generator =
      GetVariableCodeGenerator(built_in, execution_model, storage_class,
                               capabilities, extensions, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"),
            Values("Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Output"), Values("%f32arr2", "%f32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"),
            Values("Fragment", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%f32arr2", "%f32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"),
            Values("Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Private"), Values("%f32arr2", "%f32arr4"),
            Values("VUID-ClipDistance-ClipDistance-04190 "
                   "VUID-CullDistance-CullDistance-04199"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input or Output storage "
                "class."))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceFragmentOutput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("Fragment"),
            Values("Output"), Values("%f32arr4"),
            Values("VUID-ClipDistance-ClipDistance-04189 "
                   "VUID-CullDistance-CullDistance-04198"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow BuiltIn ClipDistance/CullDistance "
                "to be used for variables with Output storage class if "
                "execution model is Fragment.",
                "which is called with execution model Fragment."))));

INSTANTIATE_TEST_SUITE_P(
    VertexIdVertexInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("VertexId"), Values("Vertex"), Values("Input"), Values("%u32"),
        Values(nullptr),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Vulkan spec doesn't allow BuiltIn VertexId to be "
                          "used."))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceVertexInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("Vertex"),
            Values("Input"), Values("%f32arr4"),
            Values("VUID-ClipDistance-ClipDistance-04188 "
                   "VUID-CullDistance-CullDistance-04197"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow BuiltIn ClipDistance/CullDistance "
                "to be used for variables with Input storage class if "
                "execution model is Vertex.",
                "which is called with execution model Vertex."))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("GLCompute"),
            Values("Input", "Output"), Values("%f32arr4"),
            Values("VUID-ClipDistance-ClipDistance-04187 "
                   "VUID-CullDistance-CullDistance-04196"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be used only with Fragment, Vertex, TessellationControl, "
                "TessellationEvaluation or Geometry execution models"))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceNotArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("Fragment"),
            Values("Input"), Values("%f32vec2", "%f32vec4", "%f32"),
            Values("VUID-ClipDistance-ClipDistance-04191 "
                   "VUID-CullDistance-CullDistance-04200"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float array",
                              "is not an array"))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceNotFloatArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("Fragment"),
            Values("Input"), Values("%u32arr2", "%u64arr4"),
            Values("VUID-ClipDistance-ClipDistance-04191 "
                   "VUID-CullDistance-CullDistance-04200"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float array",
                              "components are not float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceNotF32Array,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ClipDistance", "CullDistance"), Values("Fragment"),
            Values("Input"), Values("%f64arr2", "%f64arr4"),
            Values("VUID-ClipDistance-ClipDistance-04191 "
                   "VUID-CullDistance-CullDistance-04200"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float array",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    FragCoordSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragCoord"), Values("Fragment"), Values("Input"),
            Values("%f32vec4"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FragCoordNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("FragCoord"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%f32vec4"),
        Values("VUID-FragCoord-FragCoord-04210"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    FragCoordNotInput, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragCoord"), Values("Fragment"), Values("Output"),
            Values("%f32vec4"), Values("VUID-FragCoord-FragCoord-04211"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    FragCoordNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragCoord"), Values("Fragment"), Values("Input"),
            Values("%f32arr4", "%u32vec4"),
            Values("VUID-FragCoord-FragCoord-04212"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    FragCoordNotFloatVec4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragCoord"), Values("Fragment"), Values("Input"),
            Values("%f32vec3"), Values("VUID-FragCoord-FragCoord-04212"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    FragCoordNotF32Vec4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragCoord"), Values("Fragment"), Values("Input"),
            Values("%f64vec4"), Values("VUID-FragCoord-FragCoord-04212"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    FragDepthSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragDepth"), Values("Fragment"), Values("Output"),
            Values("%f32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FragDepthNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("FragDepth"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Output"), Values("%f32"),
        Values("VUID-FragDepth-FragDepth-04213"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    FragDepthNotOutput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragDepth"), Values("Fragment"), Values("Input"),
            Values("%f32"), Values("VUID-FragDepth-FragDepth-04214"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Output storage class",
                "uses storage class Input"))));

INSTANTIATE_TEST_SUITE_P(
    FragDepthNotFloatScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragDepth"), Values("Fragment"), Values("Output"),
            Values("%f32vec4", "%u32"),
            Values("VUID-FragDepth-FragDepth-04215"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "is not a float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    FragDepthNotF32, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FragDepth"), Values("Fragment"), Values("Output"),
            Values("%f64"), Values("VUID-FragDepth-FragDepth-04215"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    FrontFacingAndHelperInvocationSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FrontFacing", "HelperInvocation"), Values("Fragment"),
            Values("Input"), Values("%bool"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FrontFacingAndHelperInvocationNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("FrontFacing", "HelperInvocation"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%bool"),
        Values("VUID-FrontFacing-FrontFacing-04229 "
               "VUID-HelperInvocation-HelperInvocation-04239"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    FrontFacingAndHelperInvocationNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FrontFacing", "HelperInvocation"), Values("Fragment"),
            Values("Output"), Values("%bool"),
            Values("VUID-FrontFacing-FrontFacing-04230 "
                   "VUID-HelperInvocation-HelperInvocation-04240"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    FrontFacingAndHelperInvocationNotBool,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("FrontFacing", "HelperInvocation"), Values("Fragment"),
            Values("Input"), Values("%f32", "%u32"),
            Values("VUID-FrontFacing-FrontFacing-04231 "
                   "VUID-HelperInvocation-HelperInvocation-04241"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a bool scalar",
                              "is not a bool scalar"))));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3Success,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("GLCompute"), Values("Input"), Values("%u32vec3"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3NotGLCompute,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("Vertex", "Fragment", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%u32vec3"),
            Values("VUID-GlobalInvocationId-GlobalInvocationId-04236 "
                   "VUID-LocalInvocationId-LocalInvocationId-04281 "
                   "VUID-NumWorkgroups-NumWorkgroups-04296 "
                   "VUID-WorkgroupId-WorkgroupId-04422"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with GLCompute, MeshNV, "
                              "TaskNV, MeshEXT or TaskEXT execution model"))));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3NotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("GLCompute"), Values("Output"), Values("%u32vec3"),
            Values("VUID-GlobalInvocationId-GlobalInvocationId-04237 "
                   "VUID-LocalInvocationId-LocalInvocationId-04282 "
                   "VUID-NumWorkgroups-NumWorkgroups-04297 "
                   "VUID-WorkgroupId-WorkgroupId-04423"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3NotIntVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("GLCompute"), Values("Input"),
            Values("%u32arr3", "%f32vec3"),
            Values("VUID-GlobalInvocationId-GlobalInvocationId-04238 "
                   "VUID-LocalInvocationId-LocalInvocationId-04283 "
                   "VUID-NumWorkgroups-NumWorkgroups-04298 "
                   "VUID-WorkgroupId-WorkgroupId-04424"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit int vector",
                              "is not an int vector"))));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3NotIntVec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("GLCompute"), Values("Input"), Values("%u32vec4"),
            Values("VUID-GlobalInvocationId-GlobalInvocationId-04238 "
                   "VUID-LocalInvocationId-LocalInvocationId-04283 "
                   "VUID-NumWorkgroups-NumWorkgroups-04298 "
                   "VUID-WorkgroupId-WorkgroupId-04424"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit int vector",
                              "has 4 components"))));

INSTANTIATE_TEST_SUITE_P(
    ComputeShaderInputInt32Vec3NotInt32Vec,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("GlobalInvocationId", "LocalInvocationId", "NumWorkgroups",
                   "WorkgroupId"),
            Values("GLCompute"), Values("Input"), Values("%u64vec3"),
            Values("VUID-GlobalInvocationId-GlobalInvocationId-04238 "
                   "VUID-LocalInvocationId-LocalInvocationId-04283 "
                   "VUID-NumWorkgroups-NumWorkgroups-04298 "
                   "VUID-WorkgroupId-WorkgroupId-04424"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit int vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    InvocationIdSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InvocationId"), Values("Geometry", "TessellationControl"),
            Values("Input"), Values("%u32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    InvocationIdInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InvocationId"),
            Values("Vertex", "Fragment", "GLCompute", "TessellationEvaluation"),
            Values("Input"), Values("%u32"),
            Values("VUID-InvocationId-InvocationId-04257"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with TessellationControl or "
                              "Geometry execution models"))));

INSTANTIATE_TEST_SUITE_P(
    InvocationIdNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InvocationId"), Values("Geometry", "TessellationControl"),
            Values("Output"), Values("%u32"),
            Values("VUID-InvocationId-InvocationId-04258"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    InvocationIdNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InvocationId"), Values("Geometry", "TessellationControl"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("VUID-InvocationId-InvocationId-04259"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    InvocationIdNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InvocationId"), Values("Geometry", "TessellationControl"),
            Values("Input"), Values("%u64"),
            Values("VUID-InvocationId-InvocationId-04259"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    InstanceIndexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InstanceIndex"), Values("Vertex"), Values("Input"),
            Values("%u32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    InstanceIndexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InstanceIndex"),
            Values("Geometry", "Fragment", "GLCompute", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%u32"),
            Values("VUID-InstanceIndex-InstanceIndex-04263"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with Vertex execution model"))));

INSTANTIATE_TEST_SUITE_P(
    InstanceIndexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InstanceIndex"), Values("Vertex"), Values("Output"),
            Values("%u32"), Values("VUID-InstanceIndex-InstanceIndex-04264"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    InstanceIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InstanceIndex"), Values("Vertex"), Values("Input"),
            Values("%f32", "%u32vec3"),
            Values("VUID-InstanceIndex-InstanceIndex-04265"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    InstanceIndexNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("InstanceIndex"), Values("Vertex"), Values("Input"),
            Values("%u64"), Values("VUID-InstanceIndex-InstanceIndex-04265"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Layer", "ViewportIndex"), Values("Fragment"),
            Values("Input"), Values("%u32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Layer", "ViewportIndex"), Values("Geometry"),
            Values("Output"), Values("%u32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("Layer", "ViewportIndex"),
        Values("TessellationControl", "GLCompute"), Values("Input"),
        Values("%u32"),
        Values("VUID-Layer-Layer-04272 VUID-ViewportIndex-ViewportIndex-04404"),
        Values(
            TestResult(SPV_ERROR_INVALID_DATA,
                       "to be used only with Vertex, TessellationEvaluation, "
                       "Geometry, or Fragment execution models"))));

INSTANTIATE_TEST_SUITE_P(
    ViewportIndexExecutionModelEnabledByCapability,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("ViewportIndex"), Values("Vertex", "TessellationEvaluation"),
            Values("Output"), Values("%u32"),
            Values("VUID-ViewportIndex-ViewportIndex-04405"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "ShaderViewportIndexLayerEXT or ShaderViewportIndex"))));

INSTANTIATE_TEST_SUITE_P(
    LayerExecutionModelEnabledByCapability,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Layer"), Values("Vertex", "TessellationEvaluation"),
            Values("Output"), Values("%u32"), Values("VUID-Layer-Layer-04273"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "ShaderViewportIndexLayerEXT or ShaderLayer"))));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexFragmentNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("Layer", "ViewportIndex"), Values("Fragment"), Values("Output"),
        Values("%u32"),
        Values("VUID-Layer-Layer-04275 VUID-ViewportIndex-ViewportIndex-04407"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Output storage class if execution model is Fragment",
                          "which is called with execution model Fragment"))));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexGeometryNotOutput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("Layer", "ViewportIndex"),
        Values("Vertex", "TessellationEvaluation", "Geometry"), Values("Input"),
        Values("%u32"),
        Values("VUID-Layer-Layer-04274 VUID-ViewportIndex-ViewportIndex-04406"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Input storage class if execution model is Vertex, "
                          "TessellationEvaluation, Geometry, MeshNV or MeshEXT",
                          "which is called with execution model"))));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("Layer", "ViewportIndex"), Values("Fragment"), Values("Input"),
        Values("%f32", "%u32vec3"),
        Values("VUID-Layer-Layer-04276 VUID-ViewportIndex-ViewportIndex-04408"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 32-bit int scalar",
                          "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    LayerAndViewportIndexNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("Layer", "ViewportIndex"), Values("Fragment"), Values("Input"),
        Values("%u64"),
        Values("VUID-Layer-Layer-04276 VUID-ViewportIndex-ViewportIndex-04408"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 32-bit int scalar",
                          "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    LayerCapability,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("Layer"), Values("Vertex"),
            Values("Output"), Values("%u32"),
            Values("OpCapability ShaderLayer\n"), Values(nullptr),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ViewportIndexCapability,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("ViewportIndex"),
            Values("Vertex"), Values("Output"), Values("%u32"),
            Values("OpCapability ShaderViewportIndex\n"), Values(nullptr),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PatchVerticesSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PatchVertices"),
            Values("TessellationEvaluation", "TessellationControl"),
            Values("Input"), Values("%u32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PatchVerticesInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PatchVertices"),
            Values("Vertex", "Fragment", "GLCompute", "Geometry"),
            Values("Input"), Values("%u32"),
            Values("VUID-PatchVertices-PatchVertices-04308"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with TessellationControl or "
                              "TessellationEvaluation execution models"))));

INSTANTIATE_TEST_SUITE_P(
    PatchVerticesNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PatchVertices"),
            Values("TessellationEvaluation", "TessellationControl"),
            Values("Output"), Values("%u32"),
            Values("VUID-PatchVertices-PatchVertices-04309"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    PatchVerticesNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PatchVertices"),
            Values("TessellationEvaluation", "TessellationControl"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("VUID-PatchVertices-PatchVertices-04310"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    PatchVerticesNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PatchVertices"),
            Values("TessellationEvaluation", "TessellationControl"),
            Values("Input"), Values("%u64"),
            Values("VUID-PatchVertices-PatchVertices-04310"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    PointCoordSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointCoord"), Values("Fragment"), Values("Input"),
            Values("%f32vec2"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PointCoordNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("PointCoord"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%f32vec2"),
        Values("VUID-PointCoord-PointCoord-04311"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    PointCoordNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointCoord"), Values("Fragment"), Values("Output"),
            Values("%f32vec2"), Values("VUID-PointCoord-PointCoord-04312"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    PointCoordNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointCoord"), Values("Fragment"), Values("Input"),
            Values("%f32arr2", "%u32vec2"),
            Values("VUID-PointCoord-PointCoord-04313"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    PointCoordNotFloatVec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointCoord"), Values("Fragment"), Values("Input"),
            Values("%f32vec3"), Values("VUID-PointCoord-PointCoord-04313"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    PointCoordNotF32Vec4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointCoord"), Values("Fragment"), Values("Input"),
            Values("%f64vec2"), Values("VUID-PointCoord-PointCoord-04313"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    PointSizeOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"),
            Values("Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Output"), Values("%f32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PointSizeInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"),
            Values("Geometry", "TessellationControl", "TessellationEvaluation"),
            Values("Input"), Values("%f32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PointSizeVertexInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"), Values("Vertex"), Values("Input"),
            Values("%f32"), Values("VUID-PointSize-PointSize-04315"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow BuiltIn PointSize "
                "to be used for variables with Input storage class if "
                "execution model is Vertex.",
                "which is called with execution model Vertex."))));

INSTANTIATE_TEST_SUITE_P(
    PointSizeInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"), Values("GLCompute", "Fragment"),
            Values("Input", "Output"), Values("%f32"),
            Values("VUID-PointSize-PointSize-04314"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be used only with Vertex, TessellationControl, "
                "TessellationEvaluation or Geometry execution models"))));

INSTANTIATE_TEST_SUITE_P(
    PointSizeNotFloatScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"), Values("Vertex"), Values("Output"),
            Values("%f32vec4", "%u32"),
            Values("VUID-PointSize-PointSize-04317"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "is not a float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    PointSizeNotF32, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PointSize"), Values("Vertex"), Values("Output"),
            Values("%f64"), Values("VUID-PointSize-PointSize-04317"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    PositionOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"),
            Values("Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Output"), Values("%f32vec4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PositionInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"),
            Values("Geometry", "TessellationControl", "TessellationEvaluation"),
            Values("Input"), Values("%f32vec4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PositionInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"),
            Values("Geometry", "TessellationControl", "TessellationEvaluation"),
            Values("Private"), Values("%f32vec4"),
            Values("VUID-Position-Position-04320"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn Position to be only used for "
                "variables with Input or Output storage class."))));

INSTANTIATE_TEST_SUITE_P(
    PositionVertexInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"), Values("Vertex"), Values("Input"),
            Values("%f32vec4"), Values("VUID-Position-Position-04319"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow BuiltIn Position "
                "to be used for variables with Input storage class if "
                "execution model is Vertex.",
                "which is called with execution model Vertex."))));

INSTANTIATE_TEST_SUITE_P(
    PositionInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"), Values("GLCompute", "Fragment"),
            Values("Input", "Output"), Values("%f32vec4"),
            Values("VUID-Position-Position-04318"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be used only with Vertex, TessellationControl, "
                "TessellationEvaluation or Geometry execution models"))));

INSTANTIATE_TEST_SUITE_P(
    PositionNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"), Values("Geometry"), Values("Input"),
            Values("%f32arr4", "%u32vec4"),
            Values("VUID-Position-Position-04321"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    PositionNotFloatVec4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"), Values("Geometry"), Values("Input"),
            Values("%f32vec3"), Values("VUID-Position-Position-04321"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    PositionNotF32Vec4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("Position"), Values("Geometry"), Values("Input"),
            Values("%f64vec4"), Values("VUID-Position-Position-04321"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PrimitiveId"),
            Values("Fragment", "TessellationControl", "TessellationEvaluation",
                   "Geometry"),
            Values("Input"), Values("%u32"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PrimitiveId"), Values("Geometry"), Values("Output"),
            Values("%u32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("PrimitiveId"), Values("Vertex", "GLCompute"), Values("Input"),
        Values("%u32"), Values("VUID-PrimitiveId-PrimitiveId-04330"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment, TessellationControl, "
                          "TessellationEvaluation, Geometry, MeshNV, MeshEXT, "
                          "IntersectionKHR, "
                          "AnyHitKHR, and ClosestHitKHR execution models"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdFragmentNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("PrimitiveId"), Values("Fragment"), Values("Output"),
        Values("%u32"), Values("VUID-PrimitiveId-PrimitiveId-04334"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Output storage class if execution model is Fragment",
                          "which is called with execution model Fragment"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdTessellationNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PrimitiveId"),
            Values("TessellationControl", "TessellationEvaluation"),
            Values("Output"), Values("%u32"),
            Values("VUID-PrimitiveId-PrimitiveId-04334"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Output storage class if execution model is Tessellation",
                "which is called with execution model Tessellation"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PrimitiveId"), Values("Fragment"), Values("Input"),
            Values("%f32", "%u32vec3"),
            Values("VUID-PrimitiveId-PrimitiveId-04337"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("PrimitiveId"), Values("Fragment"), Values("Input"),
            Values("%u64"), Values("VUID-PrimitiveId-PrimitiveId-04337"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    SampleIdSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleId"), Values("Fragment"), Values("Input"),
            Values("%u32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SampleIdInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("SampleId"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%u32"), Values("VUID-SampleId-SampleId-04354"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    SampleIdNotInput, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("SampleId"), Values("Fragment"), Values("Output"),
        Values("%u32"), Values("VUID-SampleId-SampleId-04355"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Vulkan spec allows BuiltIn SampleId to be only used "
                          "for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    SampleIdNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleId"), Values("Fragment"), Values("Input"),
            Values("%f32", "%u32vec3"), Values("VUID-SampleId-SampleId-04356"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    SampleIdNotInt32, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleId"), Values("Fragment"), Values("Input"),
            Values("%u64"), Values("VUID-SampleId-SampleId-04356"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    SampleMaskSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleMask"), Values("Fragment"), Values("Input", "Output"),
            Values("%u32arr2", "%u32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SampleMaskInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("SampleMask"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%u32arr2"),
        Values("VUID-SampleMask-SampleMask-04357"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    SampleMaskNotArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleMask"), Values("Fragment"), Values("Input"),
            Values("%f32", "%u32vec3"),
            Values("VUID-SampleMask-SampleMask-04359"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int array",
                              "is not an array"))));

INSTANTIATE_TEST_SUITE_P(
    SampleMaskNotIntArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleMask"), Values("Fragment"), Values("Input"),
            Values("%f32arr2"), Values("VUID-SampleMask-SampleMask-04359"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int array",
                              "components are not int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    SampleMaskNotInt32Array,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SampleMask"), Values("Fragment"), Values("Input"),
            Values("%u64arr2"), Values("VUID-SampleMask-SampleMask-04359"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int array",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SamplePosition"), Values("Fragment"), Values("Input"),
            Values("%f32vec2"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("SamplePosition"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%f32vec2"),
        Values("VUID-SamplePosition-SamplePosition-04360"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SamplePosition"), Values("Fragment"), Values("Output"),
            Values("%f32vec2"),
            Values("VUID-SamplePosition-SamplePosition-04361"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SamplePosition"), Values("Fragment"), Values("Input"),
            Values("%f32arr2", "%u32vec4"),
            Values("VUID-SamplePosition-SamplePosition-04362"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionNotFloatVec2,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SamplePosition"), Values("Fragment"), Values("Input"),
            Values("%f32vec3"),
            Values("VUID-SamplePosition-SamplePosition-04362"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    SamplePositionNotF32Vec2,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("SamplePosition"), Values("Fragment"), Values("Input"),
            Values("%f64vec2"),
            Values("VUID-SamplePosition-SamplePosition-04362"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    TessCoordSuccess, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessCoord"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32vec3"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    TessCoordNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("TessCoord"),
        Values("Vertex", "GLCompute", "Geometry", "TessellationControl",
               "Fragment"),
        Values("Input"), Values("%f32vec3"),
        Values("VUID-TessCoord-TessCoord-04387"),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "to be used only with TessellationEvaluation execution model"))));

INSTANTIATE_TEST_SUITE_P(
    TessCoordNotInput, ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessCoord"), Values("Fragment"), Values("Output"),
            Values("%f32vec3"), Values("VUID-TessCoord-TessCoord-04388"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    TessCoordNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessCoord"), Values("Fragment"), Values("Input"),
            Values("%f32arr3", "%u32vec4"),
            Values("VUID-TessCoord-TessCoord-04389"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    TessCoordNotFloatVec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessCoord"), Values("Fragment"), Values("Input"),
            Values("%f32vec2"), Values("VUID-TessCoord-TessCoord-04389"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit float vector",
                              "has 2 components"))));

INSTANTIATE_TEST_SUITE_P(
    TessCoordNotF32Vec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessCoord"), Values("Fragment"), Values("Input"),
            Values("%f64vec3"), Values("VUID-TessCoord-TessCoord-04389"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit float vector",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterTeseInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterTescOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationControl"),
            Values("Output"), Values("%f32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"),
            Values("Vertex", "GLCompute", "Geometry", "Fragment"),
            Values("Input"), Values("%f32arr4"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04390"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with TessellationControl or "
                              "TessellationEvaluation execution models."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterOutputTese,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Output"), Values("%f32arr4"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04392"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow TessLevelOuter/TessLevelInner to be "
                "used for variables with Output storage class if execution "
                "model is TessellationEvaluation."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterInputTesc,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationControl"),
            Values("Input"), Values("%f32arr4"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04391"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow TessLevelOuter/TessLevelInner to be "
                "used for variables with Input storage class if execution "
                "model is TessellationControl."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterNotArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32vec4", "%f32"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04393"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float array",
                              "is not an array"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterNotFloatArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Input"), Values("%u32arr4"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04393"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float array",
                              "components are not float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterNotFloatArr4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32arr3"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04393"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float array",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelOuterNotF32Arr4,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelOuter"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f64arr4"),
            Values("VUID-TessLevelOuter-TessLevelOuter-04393"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float array",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerTeseInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32arr2"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerTescOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationControl"),
            Values("Output"), Values("%f32arr2"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"),
            Values("Vertex", "GLCompute", "Geometry", "Fragment"),
            Values("Input"), Values("%f32arr2"),
            Values("VUID-TessLevelInner-TessLevelInner-04394"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with TessellationControl or "
                              "TessellationEvaluation execution models."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerOutputTese,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Output"), Values("%f32arr2"),
            Values("VUID-TessLevelInner-TessLevelInner-04396"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow TessLevelOuter/TessLevelInner to be "
                "used for variables with Output storage class if execution "
                "model is TessellationEvaluation."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerInputTesc,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationControl"),
            Values("Input"), Values("%f32arr2"),
            Values("VUID-TessLevelInner-TessLevelInner-04395"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec doesn't allow TessLevelOuter/TessLevelInner to be "
                "used for variables with Input storage class if execution "
                "model is TessellationControl."))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerNotArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32vec2", "%f32"),
            Values("VUID-TessLevelInner-TessLevelInner-04397"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float array",
                              "is not an array"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerNotFloatArray,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Input"), Values("%u32arr2"),
            Values("VUID-TessLevelInner-TessLevelInner-04397"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float array",
                              "components are not float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerNotFloatArr2,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f32arr3"),
            Values("VUID-TessLevelInner-TessLevelInner-04397"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float array",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    TessLevelInnerNotF32Arr2,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("TessLevelInner"), Values("TessellationEvaluation"),
            Values("Input"), Values("%f64arr2"),
            Values("VUID-TessLevelInner-TessLevelInner-04397"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 2-component 32-bit float array",
                              "has components with bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    VertexIndexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("VertexIndex"), Values("Vertex"), Values("Input"),
            Values("%u32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    VertexIndexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("VertexIndex"),
            Values("Fragment", "GLCompute", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%u32"),
            Values("VUID-VertexIndex-VertexIndex-04398"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with Vertex execution model"))));

INSTANTIATE_TEST_SUITE_P(
    VertexIndexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(
        Values("VertexIndex"), Values("Vertex"), Values("Output"),
        Values("%u32"), Values("VUID-VertexIndex-VertexIndex-04399"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Vulkan spec allows BuiltIn VertexIndex to be only "
                          "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    VertexIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("VertexIndex"), Values("Vertex"), Values("Input"),
            Values("%f32", "%u32vec3"),
            Values("VUID-VertexIndex-VertexIndex-04400"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    VertexIndexNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeResult,
    Combine(Values("VertexIndex"), Values("Vertex"), Values("Input"),
            Values("%u64"), Values("VUID-VertexIndex-VertexIndex-04400"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    BaseInstanceOrVertexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("BaseInstance", "BaseVertex"), Values("Vertex"),
            Values("Input"), Values("%u32"),
            Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    BaseInstanceOrVertexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("BaseInstance", "BaseVertex"),
            Values("Fragment", "GLCompute", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%u32"),
            Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values("VUID-BaseInstance-BaseInstance-04181 "
                   "VUID-BaseVertex-BaseVertex-04184"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with Vertex execution model"))));

INSTANTIATE_TEST_SUITE_P(
    BaseInstanceOrVertexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("BaseInstance", "BaseVertex"), Values("Vertex"),
            Values("Output"), Values("%u32"),
            Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values("VUID-BaseInstance-BaseInstance-04182 "
                   "VUID-BaseVertex-BaseVertex-04185"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    BaseInstanceOrVertexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("BaseInstance", "BaseVertex"), Values("Vertex"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values("VUID-BaseInstance-BaseInstance-04183 "
                   "VUID-BaseVertex-BaseVertex-04186"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    DrawIndexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DrawIndex"), Values("Vertex"), Values("Input"),
            Values("%u32"), Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    DrawIndexMeshSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("DrawIndex"), Values("MeshNV", "TaskNV"), Values("Input"),
        Values("%u32"), Values("OpCapability MeshShadingNV\n"),
        Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\nOpExtension "
               "\"SPV_NV_mesh_shader\"\n"),
        Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    DrawIndexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("DrawIndex"),
        Values("Fragment", "GLCompute", "Geometry", "TessellationControl",
               "TessellationEvaluation"),
        Values("Input"), Values("%u32"),
        Values("OpCapability DrawParameters\n"),
        Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
        Values("VUID-DrawIndex-DrawIndex-04207"),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "to be used only with Vertex, MeshNV, TaskNV , MeshEXT or TaskEXT "
            "execution model"))));

INSTANTIATE_TEST_SUITE_P(
    DrawIndexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DrawIndex"), Values("Vertex"), Values("Output"),
            Values("%u32"), Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values("VUID-DrawIndex-DrawIndex-04208"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    DrawIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DrawIndex"), Values("Vertex"), Values("Input"),
            Values("%f32", "%u32vec3"), Values("OpCapability DrawParameters\n"),
            Values("OpExtension \"SPV_KHR_shader_draw_parameters\"\n"),
            Values("VUID-DrawIndex-DrawIndex-04209"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    ViewIndexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ViewIndex"),
            Values("Fragment", "Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation"),
            Values("Input"), Values("%u32"), Values("OpCapability MultiView\n"),
            Values("OpExtension \"SPV_KHR_multiview\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ViewIndexInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ViewIndex"), Values("GLCompute"), Values("Input"),
            Values("%u32"), Values("OpCapability MultiView\n"),
            Values("OpExtension \"SPV_KHR_multiview\"\n"),
            Values("VUID-ViewIndex-ViewIndex-04401"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be not be used with GLCompute execution model"))));

INSTANTIATE_TEST_SUITE_P(
    ViewIndexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ViewIndex"), Values("Vertex"), Values("Output"),
            Values("%u32"), Values("OpCapability MultiView\n"),
            Values("OpExtension \"SPV_KHR_multiview\"\n"),
            Values("VUID-ViewIndex-ViewIndex-04402"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    ViewIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ViewIndex"), Values("Vertex"), Values("Input"),
            Values("%f32", "%u32vec3"), Values("OpCapability MultiView\n"),
            Values("OpExtension \"SPV_KHR_multiview\"\n"),
            Values("VUID-ViewIndex-ViewIndex-04403"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    DeviceIndexSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DeviceIndex"),
            Values("Fragment", "Vertex", "Geometry", "TessellationControl",
                   "TessellationEvaluation", "GLCompute"),
            Values("Input"), Values("%u32"),
            Values("OpCapability DeviceGroup\n"),
            Values("OpExtension \"SPV_KHR_device_group\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    DeviceIndexNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DeviceIndex"), Values("Fragment", "Vertex", "GLCompute"),
            Values("Output"), Values("%u32"),
            Values("OpCapability DeviceGroup\n"),
            Values("OpExtension \"SPV_KHR_device_group\"\n"),
            Values("VUID-DeviceIndex-DeviceIndex-04205"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    DeviceIndexNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("DeviceIndex"), Values("Fragment", "Vertex", "GLCompute"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability DeviceGroup\n"),
            Values("OpExtension \"SPV_KHR_device_group\"\n"),
            Values("VUID-DeviceIndex-DeviceIndex-04206"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// Test HitKind in NV RT shaders
INSTANTIATE_TEST_SUITE_P(
    HitKindNVSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitKindNV"),
            Values("AnyHitNV", "ClosestHitNV"), Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingNV\n"),
            Values("OpExtension \"SPV_NV_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

// HitKind is valid in AH, CH shaders as input i32 scalar
INSTANTIATE_TEST_SUITE_P(
    HitKindSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitKindKHR"),
            Values("AnyHitKHR", "ClosestHitKHR"), Values("Input"),
            Values("%u32"), Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    HitKindNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitKindKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "IntersectionKHR",
                   "MissKHR", "CallableKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-HitKindKHR-HitKindKHR-04242"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    HitKindNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitKindKHR"),
            Values("AnyHitKHR", "ClosestHitKHR"), Values("Output"),
            Values("%u32"), Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-HitKindKHR-HitKindKHR-04243"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    HitKindNotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitKindKHR"),
            Values("AnyHitKHR", "ClosestHitKHR"), Values("Input"),
            Values("%f32", "%u32vec3"), Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-HitKindKHR-HitKindKHR-04244"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// Ensure HitT is not supported in KHR RT shaders
INSTANTIATE_TEST_SUITE_P(
    HitTNVNotSupportedInKHR,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitTNV"),
            Values("AnyHitKHR", "ClosestHitKHR"), Values("Input"),
            Values("%u32"), Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult(
                SPV_ERROR_INVALID_CAPABILITY,
                "of MemberDecorate requires one of these capabilities"))));

// HitT is valid in AH, CH shaders as input f32 scalar (NV RT only)
INSTANTIATE_TEST_SUITE_P(
    HitTNVSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitTNV"),
            Values("AnyHitNV", "ClosestHitNV"), Values("Input"), Values("%f32"),
            Values("OpCapability RayTracingNV\n"),
            Values("OpExtension \"SPV_NV_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    HitTNVNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitTNV"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationNV", "IntersectionNV", "MissNV",
                   "CallableNV"),
            Values("Input"), Values("%f32"),
            Values("OpCapability RayTracingNV\n"),
            Values("OpExtension \"SPV_NV_ray_tracing\"\n"),
            Values("VUID-HitTNV-HitTNV-04245"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    HitTNVNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitTNV"),
            Values("AnyHitNV", "ClosestHitNV"), Values("Output"),
            Values("%f32"), Values("OpCapability RayTracingNV\n"),
            Values("OpExtension \"SPV_NV_ray_tracing\"\n"),
            Values("VUID-HitTNV-HitTNV-04246"),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));
INSTANTIATE_TEST_SUITE_P(
    HitTNVNotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("HitTNV"),
            Values("AnyHitNV", "ClosestHitNV"), Values("Input"),
            Values("%u32", "%f32vec3"), Values("OpCapability RayTracingNV\n"),
            Values("OpExtension \"SPV_NV_ray_tracing\"\n"),
            Values("VUID-HitTNV-HitTNV-04247"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "is not a float scalar"))));

// InstanceCustomIndexKHR, InstanceId, PrimitiveId, RayGeometryIndexKHR are
// valid in IS, AH, CH shaders as input i32 scalars
INSTANTIATE_TEST_SUITE_P(
    RTBuiltIn3StageI32Success,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("InstanceCustomIndexKHR", "RayGeometryIndexKHR",
                   "InstanceId", "PrimitiveId"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    RTBuiltIn3StageI32NotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("InstanceCustomIndexKHR", "RayGeometryIndexKHR",
                   "InstanceId"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "MissKHR", "CallableKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-InstanceCustomIndexKHR-InstanceCustomIndexKHR-04251 "
                   "VUID-RayGeometryIndexKHR-RayGeometryIndexKHR-04345 "
                   "VUID-InstanceId-InstanceId-04254 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    RTBuiltIn3StageI32NotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("InstanceCustomIndexKHR", "RayGeometryIndexKHR",
                   "InstanceId"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Output"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-InstanceCustomIndexKHR-InstanceCustomIndexKHR-04252 "
                   "VUID-RayGeometryIndexKHR-RayGeometryIndexKHR-04346 "
                   "VUID-InstanceId-InstanceId-04255 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    RTBuiltIn3StageI32NotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("InstanceCustomIndexKHR", "RayGeometryIndexKHR",
                   "InstanceId"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-InstanceCustomIndexKHR-InstanceCustomIndexKHR-04253 "
                   "VUID-RayGeometryIndexKHR-RayGeometryIndexKHR-04347 "
                   "VUID-InstanceId-InstanceId-04256 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// PrimitiveId needs special negative testing because it has non-RT uses
INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdRTNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values(SPV_ENV_VULKAN_1_2), Values("PrimitiveId"),
        Values("RayGenerationKHR", "MissKHR", "CallableKHR"), Values("Input"),
        Values("%u32"), Values("OpCapability RayTracingKHR\n"),
        Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
        Values("VUID-PrimitiveId-PrimitiveId-04330"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "to be used only with Fragment, TessellationControl, "
                          "TessellationEvaluation, Geometry, MeshNV, MeshEXT, "
                          "IntersectionKHR, "
                          "AnyHitKHR, and ClosestHitKHR execution models"))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdRTNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("PrimitiveId"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Output"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-PrimitiveId-PrimitiveId-04334"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Output storage class if execution model is "))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveIdRTNotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("PrimitiveId"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-PrimitiveId-PrimitiveId-04337"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// ObjectRayDirectionKHR and ObjectRayOriginKHR valid
// in IS, AH, CH shaders as input 32-bit float vec3
INSTANTIATE_TEST_SUITE_P(
    ObjectRayDirectionAndOriginSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectRayDirectionKHR", "ObjectRayOriginKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ObjectRayDirectionAndOriginNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectRayDirectionKHR", "ObjectRayOriginKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "MissKHR", "CallableKHR"),
            Values("Input"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-ObjectRayDirectionKHR-ObjectRayDirectionKHR-04299 "
                   "VUID-ObjectRayOriginKHR-ObjectRayOriginKHR-04302 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    ObjectRayDirectionAndOriginNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectRayDirectionKHR", "ObjectRayOriginKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Output"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-ObjectRayDirectionKHR-ObjectRayDirectionKHR-04300 "
                   "VUID-ObjectRayOriginKHR-ObjectRayOriginKHR-04303 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    ObjectRayDirectionAndOriginNotFloatVec3,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values(SPV_ENV_VULKAN_1_2),
        Values("ObjectRayDirectionKHR", "ObjectRayOriginKHR"),
        Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
        Values("Input"), Values("%u32vec3", "%f32", "%f32vec2", "%f32vec4"),
        Values("OpCapability RayTracingKHR\n"),
        Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
        Values("VUID-ObjectRayDirectionKHR-ObjectRayDirectionKHR-04301 "
               "VUID-ObjectRayOriginKHR-ObjectRayOriginKHR-04304 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 3-component 32-bit float vector"))));

// ObjectToWorldKHR and WorldToObjectKHR valid
// in IS, AH, CH shaders as input mat4x3
INSTANTIATE_TEST_SUITE_P(
    RTObjectMatrixSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectToWorldKHR", "WorldToObjectKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%f32mat34"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    RTObjectMatrixNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectToWorldKHR", "WorldToObjectKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "MissKHR", "CallableKHR"),
            Values("Input"), Values("%f32mat34"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-ObjectToWorldKHR-ObjectToWorldKHR-04305 "
                   "VUID-WorldToObjectKHR-WorldToObjectKHR-04434 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    RTObjectMatrixNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectToWorldKHR", "WorldToObjectKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Output"), Values("%f32mat34"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-ObjectToWorldKHR-ObjectToWorldKHR-04306 "
                   "VUID-WorldToObjectKHR-WorldToObjectKHR-04435 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    RTObjectMatrixNotMat4x3,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("ObjectToWorldKHR", "WorldToObjectKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR"),
            Values("Input"), Values("%f32mat43", "%f32mat44", "%f32vec4"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-ObjectToWorldKHR-ObjectToWorldKHR-04307 "
                   "VUID-WorldToObjectKHR-WorldToObjectKHR-04436 "),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "variable needs to be a matrix with "
                "4 columns of 3-component vectors of 32-bit floats"))));

// IncomingRayFlagsKHR is valid
// in IS, AH, CH, MS shaders as an input i32 scalar
INSTANTIATE_TEST_SUITE_P(
    IncomingRayFlagsSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("IncomingRayFlagsKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    IncomingRayFlagsNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("IncomingRayFlagsKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "CallableKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04248 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04348 "
                   "VUID-RayTminKHR-RayTminKHR-04351 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    IncomingRayFlagsNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("IncomingRayFlagsKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Output"), Values("%u32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04249 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04349 "
                   "VUID-RayTminKHR-RayTminKHR-04352 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));
INSTANTIATE_TEST_SUITE_P(
    IncomingRayFlagsNotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("IncomingRayFlagsKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04250 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04350 "
                   "VUID-RayTminKHR-RayTminKHR-04353 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// CullMaskKHR is valid
// in IS, AH, CH, MS shaders as an input i32 scalar
INSTANTIATE_TEST_SUITE_P(
    CullMaskSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("CullMaskKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\nOpCapability RayCullMaskKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\nOpExtension "
                   "\"SPV_KHR_ray_cull_mask\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    CullMaskNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("CullMaskKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "CallableKHR"),
            Values("Input"), Values("%u32"),
            Values("OpCapability RayTracingKHR\nOpCapability RayCullMaskKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\nOpExtension "
                   "\"SPV_KHR_ray_cull_mask\"\n"),
            Values("VUID-CullMaskKHR-CullMaskKHR-06735 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04348 "
                   "VUID-RayTminKHR-RayTminKHR-04351 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    ICullMaskNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("CullMaskKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Output"), Values("%u32"),
            Values("OpCapability RayTracingKHR\nOpCapability RayCullMaskKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\nOpExtension "
                   "\"SPV_KHR_ray_cull_mask\"\n"),
            Values("VUID-CullMaskKHR-CullMaskKHR-06736 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04349 "
                   "VUID-RayTminKHR-RayTminKHR-04352 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));
INSTANTIATE_TEST_SUITE_P(
    CullMaskNotIntScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("CullMaskKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability RayTracingKHR\nOpCapability RayCullMaskKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\nOpExtension "
                   "\"SPV_KHR_ray_cull_mask\"\n"),
            Values("VUID-CullMaskKHR-CullMaskKHR-06737 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04350 "
                   "VUID-RayTminKHR-RayTminKHR-04353 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

// RayTmaxKHR, RayTminKHR are all valid
// in IS, AH, CH, MS shaders as input f32 scalars
INSTANTIATE_TEST_SUITE_P(
    RayTSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("RayTmaxKHR", "RayTminKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%f32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    RayTNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("RayTmaxKHR", "RayTminKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "CallableKHR"),
            Values("Input"), Values("%f32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04248 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04348 "
                   "VUID-RayTminKHR-RayTminKHR-04351 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    RayTNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("RayTmaxKHR", "RayTminKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Output"), Values("%f32"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04249 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04349 "
                   "VUID-RayTminKHR-RayTminKHR-04352 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));
INSTANTIATE_TEST_SUITE_P(
    RayTNotFloatScalar,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("RayTmaxKHR", "RayTminKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%u32", "%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-IncomingRayFlagsKHR-IncomingRayFlagsKHR-04250 "
                   "VUID-RayTmaxKHR-RayTmaxKHR-04350 "
                   "VUID-RayTminKHR-RayTminKHR-04353 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "is not a float scalar"))));

// WorldRayDirectionKHR and WorldRayOriginKHR are valid
// in IS, AH, CH, MS shaders as input 32-bit float vec3
INSTANTIATE_TEST_SUITE_P(
    WorldRayDirectionAndOriginSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("WorldRayDirectionKHR", "WorldRayOriginKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Input"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    WorldRayDirectionAndOriginNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("WorldRayDirectionKHR", "WorldRayOriginKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute", "RayGenerationKHR", "CallableKHR"),
            Values("Input"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-WorldRayDirectionKHR-WorldRayDirectionKHR-04428 "
                   "VUID-WorldRayOriginKHR-WorldRayOriginKHR-04431 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    WorldRayDirectionAndOriginNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2),
            Values("WorldRayDirectionKHR", "WorldRayOriginKHR"),
            Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
            Values("Output"), Values("%f32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-WorldRayDirectionKHR-WorldRayDirectionKHR-04429 "
                   "VUID-WorldRayOriginKHR-WorldRayOriginKHR-04432 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    WorldRayDirectionAndOriginNotFloatVec3,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values(SPV_ENV_VULKAN_1_2),
        Values("WorldRayDirectionKHR", "WorldRayOriginKHR"),
        Values("AnyHitKHR", "ClosestHitKHR", "IntersectionKHR", "MissKHR"),
        Values("Input"), Values("%u32vec3", "%f32", "%f32vec2", "%f32vec4"),
        Values("OpCapability RayTracingKHR\n"),
        Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
        Values("VUID-WorldRayDirectionKHR-WorldRayDirectionKHR-04430 "
               "VUID-WorldRayOriginKHR-WorldRayOriginKHR-04433 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 3-component 32-bit float vector"))));

// LaunchIdKHR and LaunchSizeKHR are valid
// in RG, IS, AH, CH, MS shaders as input 32-bit ivec3
INSTANTIATE_TEST_SUITE_P(
    LaunchRTSuccess,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("LaunchIdKHR", "LaunchSizeKHR"),
            Values("RayGenerationKHR", "AnyHitKHR", "ClosestHitKHR",
                   "IntersectionKHR", "MissKHR", "CallableKHR"),
            Values("Input"), Values("%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    LaunchRTNotExecutionMode,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("LaunchIdKHR", "LaunchSizeKHR"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "Fragment",
                   "GLCompute"),
            Values("Input"), Values("%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-LaunchIdKHR-LaunchIdKHR-04266 "
                   "VUID-LaunchSizeKHR-LaunchSizeKHR-04269 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec does not allow BuiltIn",
                              "to be used with the execution model"))));

INSTANTIATE_TEST_SUITE_P(
    LaunchRTNotInput,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("LaunchIdKHR", "LaunchSizeKHR"),
            Values("RayGenerationKHR", "AnyHitKHR", "ClosestHitKHR",
                   "IntersectionKHR", "MissKHR", "CallableKHR"),
            Values("Output"), Values("%u32vec3"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-LaunchIdKHR-LaunchIdKHR-04267 "
                   "VUID-LaunchSizeKHR-LaunchSizeKHR-04270 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows",
                              "used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    LaunchRTNotIntVec3,
    ValidateGenericCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values(SPV_ENV_VULKAN_1_2), Values("LaunchIdKHR", "LaunchSizeKHR"),
            Values("RayGenerationKHR", "AnyHitKHR", "ClosestHitKHR",
                   "IntersectionKHR", "MissKHR", "CallableKHR"),
            Values("Input"), Values("%f32vec3", "%u32", "%u32vec2", "%u32vec4"),
            Values("OpCapability RayTracingKHR\n"),
            Values("OpExtension \"SPV_KHR_ray_tracing\"\n"),
            Values("VUID-LaunchIdKHR-LaunchIdKHR-04268 "
                   "VUID-LaunchSizeKHR-LaunchSizeKHR-04271 "),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 3-component 32-bit int vector"))));

CodeGenerator GetArrayedVariableCodeGenerator(const char* const built_in,
                                              const char* const execution_model,
                                              const char* const storage_class,
                                              const char* const data_type) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = "OpDecorate %built_in_var BuiltIn ";
  generator.before_types_ += built_in;
  generator.before_types_ += "\n";

  std::ostringstream after_types;
  after_types << "%built_in_array = OpTypeArray " << data_type << " %u32_3\n";
  if (InitializerRequired(storage_class)) {
    after_types << "%built_in_array_null = OpConstantNull %built_in_array\n";
  }

  after_types << "%built_in_ptr = OpTypePointer " << storage_class
              << " %built_in_array\n";
  after_types << "%built_in_var = OpVariable %built_in_ptr " << storage_class;
  if (InitializerRequired(storage_class)) {
    after_types << " %built_in_array_null";
  }
  after_types << "\n";
  generator.after_types_ = after_types.str();

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = execution_model;
  entry_point.interfaces = "%built_in_var";
  // Any kind of reference would do.
  entry_point.body = R"(
%val = OpBitcast %u32 %built_in_var
)";

  std::ostringstream execution_modes;
  if (0 == std::strcmp(execution_model, "Fragment")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OriginUpperLeft\n";
    if (0 == std::strcmp(built_in, "FragDepth")) {
      execution_modes << "OpExecutionMode %" << entry_point.name
                      << " DepthReplacing\n";
    }
  }
  if (0 == std::strcmp(execution_model, "Geometry")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " InputPoints\n";
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OutputPoints\n";
  }
  if (0 == std::strcmp(execution_model, "GLCompute")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " LocalSize 1 1 1\n";
  }
  entry_point.execution_modes = execution_modes.str();

  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_P(ValidateVulkanCombineBuiltInArrayedVariable, Variable) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const vuid = std::get<4>(GetParam());
  const TestResult& test_result = std::get<5>(GetParam());

  CodeGenerator generator = GetArrayedVariableCodeGenerator(
      built_in, execution_model, storage_class, data_type);

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

INSTANTIATE_TEST_SUITE_P(
    PointSizeArrayedF32TessControl, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("PointSize"), Values("TessellationControl"), Values("Input"),
            Values("%f32"), Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PointSizeArrayedF64TessControl, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("PointSize"), Values("TessellationControl"), Values("Input"),
            Values("%f64"), Values("VUID-PointSize-PointSize-04317"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    PointSizeArrayedF32Vertex, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("PointSize"), Values("Vertex"), Values("Output"),
            Values("%f32"), Values("VUID-PointSize-PointSize-04317"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float scalar",
                              "is not a float scalar"))));

INSTANTIATE_TEST_SUITE_P(PositionArrayedF32Vec4TessControl,
                         ValidateVulkanCombineBuiltInArrayedVariable,
                         Combine(Values("Position"),
                                 Values("TessellationControl"), Values("Input"),
                                 Values("%f32vec4"), Values(nullptr),
                                 Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PositionArrayedF32Vec3TessControl,
    ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("Position"), Values("TessellationControl"), Values("Input"),
            Values("%f32vec3"), Values("VUID-Position-Position-04321"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "has 3 components"))));

INSTANTIATE_TEST_SUITE_P(
    PositionArrayedF32Vec4Vertex, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("Position"), Values("Vertex"), Values("Output"),
            Values("%f32vec4"), Values("VUID-Position-Position-04321"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit float vector",
                              "is not a float vector"))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceOutputSuccess,
    ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("ClipDistance", "CullDistance"),
            Values("Geometry", "TessellationControl", "TessellationEvaluation"),
            Values("Output"), Values("%f32arr2", "%f32arr4"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceVertexInput, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("ClipDistance", "CullDistance"), Values("Fragment"),
            Values("Input"), Values("%f32arr4"),
            Values("VUID-ClipDistance-ClipDistance-04191 "
                   "VUID-CullDistance-CullDistance-04200"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float array",
                              "components are not float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    ClipAndCullDistanceNotArray, ValidateVulkanCombineBuiltInArrayedVariable,
    Combine(Values("ClipDistance", "CullDistance"),
            Values("Geometry", "TessellationControl", "TessellationEvaluation"),
            Values("Input"), Values("%f32vec2", "%f32vec4"),
            Values("VUID-ClipDistance-ClipDistance-04191 "
                   "VUID-CullDistance-CullDistance-04200"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit float array",
                              "components are not float scalar"))));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%u32"),
            Values("OpCapability ShaderSMBuiltinsNV\n"),
            Values("OpExtension \"SPV_NV_shader_sm_builtins\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsInputMeshSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
        Values("MeshNV", "TaskNV"), Values("Input"), Values("%u32"),
        Values("OpCapability ShaderSMBuiltinsNV\nOpCapability MeshShadingNV\n"),
        Values("OpExtension \"SPV_NV_shader_sm_builtins\"\nOpExtension "
               "\"SPV_NV_mesh_shader\"\n"),
        Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsInputRaySuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
        Values("RayGenerationNV", "IntersectionNV", "AnyHitNV", "ClosestHitNV",
               "MissNV", "CallableNV"),
        Values("Input"), Values("%u32"),
        Values("OpCapability ShaderSMBuiltinsNV\nOpCapability RayTracingNV\n"),
        Values("OpExtension \"SPV_NV_shader_sm_builtins\"\nOpExtension "
               "\"SPV_NV_ray_tracing\"\n"),
        Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Output"), Values("%u32"),
            Values("OpCapability ShaderSMBuiltinsNV\n"),
            Values("OpExtension \"SPV_NV_shader_sm_builtins\"\n"),
            Values(nullptr),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability ShaderSMBuiltinsNV\n"),
            Values("OpExtension \"SPV_NV_shader_sm_builtins\"\n"),
            Values(nullptr),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    SMBuiltinsNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("SMCountNV", "SMIDNV", "WarpsPerSMNV", "WarpIDNV"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%u64"),
            Values("OpCapability ShaderSMBuiltinsNV\n"),
            Values("OpExtension \"SPV_NV_shader_sm_builtins\"\n"),
            Values(nullptr),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

INSTANTIATE_TEST_SUITE_P(
    ArmCoreBuiltinsInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("CoreIDARM", "CoreCountARM", "CoreMaxIDARM", "WarpIDARM",
                   "WarpMaxIDARM"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%u32"),
            Values("OpCapability CoreBuiltinsARM\n"),
            Values("OpExtension \"SPV_ARM_core_builtins\"\n"), Values(nullptr),
            Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ArmCoreBuiltinsNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("CoreIDARM", "CoreCountARM", "CoreMaxIDARM", "WarpIDARM",
                   "WarpMaxIDARM"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Output"), Values("%u32"),
            Values("OpCapability CoreBuiltinsARM\n"),
            Values("OpExtension \"SPV_ARM_core_builtins\"\n"), Values(nullptr),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class",
                "uses storage class Output"))));

INSTANTIATE_TEST_SUITE_P(
    ArmCoreBuiltinsNotIntScalar,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("CoreIDARM", "CoreCountARM", "CoreMaxIDARM", "WarpIDARM",
                   "WarpMaxIDARM"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%f32", "%u32vec3"),
            Values("OpCapability CoreBuiltinsARM\n"),
            Values("OpExtension \"SPV_ARM_core_builtins\"\n"), Values(nullptr),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "is not an int scalar"))));

INSTANTIATE_TEST_SUITE_P(
    ArmCoreBuiltinsNotInt32,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("CoreIDARM", "CoreCountARM", "CoreMaxIDARM", "WarpIDARM",
                   "WarpMaxIDARM"),
            Values("Vertex", "Fragment", "TessellationControl",
                   "TessellationEvaluation", "Geometry", "GLCompute"),
            Values("Input"), Values("%u64"),
            Values("OpCapability CoreBuiltinsARM\n"),
            Values("OpExtension \"SPV_ARM_core_builtins\"\n"), Values(nullptr),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int scalar",
                              "has bit width 64"))));

CodeGenerator GetWorkgroupSizeSuccessGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec3 %u32_1 %u32_1 %u32_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %u32vec3 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanWorkgroupSizeSuccess) {
  CodeGenerator generator = GetWorkgroupSizeSuccessGenerator();
  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

CodeGenerator GetWorkgroupSizeFragmentGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec3 %u32_1 %u32_1 %u32_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Fragment";
  entry_point.execution_modes = "OpExecutionMode %main OriginUpperLeft";
  entry_point.body = R"(
%copy = OpCopyObject %u32vec3 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanWorkgroupSizeFragment) {
  CodeGenerator generator = GetWorkgroupSizeFragmentGenerator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec allows BuiltIn WorkgroupSize to be used "
                        "only with GLCompute, MeshNV, TaskNV, MeshEXT or "
                        "TaskEXT execution model"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("is referencing ID <2> (OpConstantComposite) which is "
                        "decorated with BuiltIn WorkgroupSize in function <1> "
                        "called with execution model Fragment"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-WorkgroupSize-WorkgroupSize-04425 "
                      "VUID-WorkgroupSize-WorkgroupSize-04427"));
}

TEST_F(ValidateBuiltIns, WorkgroupSizeNotConstant) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %copy BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec3 %u32_1 %u32_1 %u32_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %u32vec3 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("BuiltIns can only target variables, structure "
                        "members or constants"));
}

CodeGenerator GetWorkgroupSizeNotVectorGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstant %u32 16
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %u32 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanWorkgroupSizeNotVector) {
  CodeGenerator generator = GetWorkgroupSizeNotVectorGenerator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("According to the Vulkan spec BuiltIn WorkgroupSize "
                        "variable needs to be a 3-component 32-bit int vector. "
                        "ID <2> (OpConstant) is not an int vector."));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-WorkgroupSize-WorkgroupSize-04427"));
}

CodeGenerator GetWorkgroupSizeNotIntVectorGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %f32vec3 %f32_1 %f32_1 %f32_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %f32vec3 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanWorkgroupSizeNotIntVector) {
  CodeGenerator generator = GetWorkgroupSizeNotIntVectorGenerator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("According to the Vulkan spec BuiltIn WorkgroupSize "
                        "variable needs to be a 3-component 32-bit int vector. "
                        "ID <2> (OpConstantComposite) is not an int vector."));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-WorkgroupSize-WorkgroupSize-04427"));
}

CodeGenerator GetWorkgroupSizeNotVec3Generator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec2 %u32_1 %u32_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %u32vec2 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanWorkgroupSizeNotVec3) {
  CodeGenerator generator = GetWorkgroupSizeNotVec3Generator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("According to the Vulkan spec BuiltIn WorkgroupSize "
                        "variable needs to be a 3-component 32-bit int vector. "
                        "ID <2> (OpConstantComposite) has 2 components."));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-WorkgroupSize-WorkgroupSize-04427"));
}

TEST_F(ValidateBuiltIns, WorkgroupSizeNotInt32Vec) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u64vec3 %u64_1 %u64_1 %u64_1
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
%copy = OpCopyObject %u64vec3 %workgroup_size
)";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("According to the Vulkan spec BuiltIn WorkgroupSize variable "
                "needs to be a 3-component 32-bit int vector. ID <2> "
                "(OpConstantComposite) has components with bit width 64."));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-WorkgroupSize-WorkgroupSize-04427"));
}

TEST_F(ValidateBuiltIns, WorkgroupSizePrivateVar) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec3 %u32_1 %u32_1 %u32_1
%private_ptr_u32vec3 = OpTypePointer Private %u32vec3
%var = OpVariable %private_ptr_u32vec3 Private %workgroup_size
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.body = R"(
)";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateBuiltIns, GeometryPositionInOutSuccess) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %input_type Block
OpMemberDecorate %input_type 0 BuiltIn Position
OpDecorate %output_type Block
OpMemberDecorate %output_type 0 BuiltIn Position
)";

  generator.after_types_ = R"(
%input_type = OpTypeStruct %f32vec4
%arrayed_input_type = OpTypeArray %input_type %u32_3
%input_ptr = OpTypePointer Input %arrayed_input_type
%input = OpVariable %input_ptr Input
%input_f32vec4_ptr = OpTypePointer Input %f32vec4
%output_type = OpTypeStruct %f32vec4
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output
%output_f32vec4_ptr = OpTypePointer Output %f32vec4
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Geometry";
  entry_point.interfaces = "%input %output";
  entry_point.body = R"(
%input_pos = OpAccessChain %input_f32vec4_ptr %input %u32_0 %u32_0
%output_pos = OpAccessChain %output_f32vec4_ptr %output %u32_0
%pos = OpLoad %f32vec4 %input_pos
OpStore %output_pos %pos
)";
  generator.entry_points_.push_back(std::move(entry_point));
  generator.entry_points_[0].execution_modes =
      "OpExecutionMode %main InputPoints\nOpExecutionMode %main OutputPoints\n";

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateBuiltIns, WorkgroupIdNotVec3) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %workgroup_size BuiltIn WorkgroupSize
OpDecorate %workgroup_id BuiltIn WorkgroupId
)";

  generator.after_types_ = R"(
%workgroup_size = OpConstantComposite %u32vec3 %u32_1 %u32_1 %u32_1
     %input_ptr = OpTypePointer Input %u32vec2
  %workgroup_id = OpVariable %input_ptr Input
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "GLCompute";
  entry_point.interfaces = "%workgroup_id";
  entry_point.body = R"(
%copy_size = OpCopyObject %u32vec3 %workgroup_size
  %load_id = OpLoad %u32vec2 %workgroup_id
)";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("According to the Vulkan spec BuiltIn WorkgroupId "
                        "variable needs to be a 3-component 32-bit int vector. "
                        "ID <2> (OpVariable) has 2 components."));
}

TEST_F(ValidateBuiltIns, TwoBuiltInsFirstFails) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %input_type Block
OpDecorate %output_type Block
OpMemberDecorate %input_type 0 BuiltIn FragCoord
OpMemberDecorate %output_type 0 BuiltIn Position
)";

  generator.after_types_ = R"(
%input_type = OpTypeStruct %f32vec4
%input_ptr = OpTypePointer Input %input_type
%input = OpVariable %input_ptr Input
%input_f32vec4_ptr = OpTypePointer Input %f32vec4
%output_type = OpTypeStruct %f32vec4
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output
%output_f32vec4_ptr = OpTypePointer Output %f32vec4
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Geometry";
  entry_point.interfaces = "%input %output";
  entry_point.body = R"(
%input_pos = OpAccessChain %input_f32vec4_ptr %input %u32_0
%output_pos = OpAccessChain %output_f32vec4_ptr %output %u32_0
%pos = OpLoad %f32vec4 %input_pos
OpStore %output_pos %pos
)";
  generator.entry_points_.push_back(std::move(entry_point));
  generator.entry_points_[0].execution_modes =
      "OpExecutionMode %main InputPoints\nOpExecutionMode %main OutputPoints\n";

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec allows BuiltIn FragCoord to be used only "
                        "with Fragment execution model"));
}

TEST_F(ValidateBuiltIns, TwoBuiltInsSecondFails) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %input_type Block
OpDecorate %output_type Block
OpMemberDecorate %input_type 0 BuiltIn Position
OpMemberDecorate %output_type 0 BuiltIn FragCoord
)";

  generator.after_types_ = R"(
%input_type = OpTypeStruct %f32vec4
%input_ptr = OpTypePointer Input %input_type
%input = OpVariable %input_ptr Input
%input_f32vec4_ptr = OpTypePointer Input %f32vec4
%output_type = OpTypeStruct %f32vec4
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output
%output_f32vec4_ptr = OpTypePointer Output %f32vec4
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Geometry";
  entry_point.interfaces = "%input %output";
  entry_point.body = R"(
%input_pos = OpAccessChain %input_f32vec4_ptr %input %u32_0
%output_pos = OpAccessChain %output_f32vec4_ptr %output %u32_0
%pos = OpLoad %f32vec4 %input_pos
OpStore %output_pos %pos
)";
  generator.entry_points_.push_back(std::move(entry_point));
  generator.entry_points_[0].execution_modes =
      "OpExecutionMode %main InputPoints\nOpExecutionMode %main OutputPoints\n";

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec allows BuiltIn FragCoord to be only used "
                        "for variables with Input storage class"));
}

TEST_F(ValidateBuiltIns, VertexPositionVariableSuccess) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %position BuiltIn Position
)";

  generator.after_types_ = R"(
%f32vec4_ptr_output = OpTypePointer Output %f32vec4
%position = OpVariable %f32vec4_ptr_output Output
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Vertex";
  entry_point.interfaces = "%position";
  entry_point.body = R"(
OpStore %position %f32vec4_0123
)";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateBuiltIns, FragmentPositionTwoEntryPoints) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = R"(
OpDecorate %output_type Block
OpMemberDecorate %output_type 0 BuiltIn Position
)";

  generator.after_types_ = R"(
%output_type = OpTypeStruct %f32vec4
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output
%output_f32vec4_ptr = OpTypePointer Output %f32vec4
)";

  EntryPoint entry_point;
  entry_point.name = "vmain";
  entry_point.execution_model = "Vertex";
  entry_point.interfaces = "%output";
  entry_point.body = R"(
%val1 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  entry_point.name = "fmain";
  entry_point.execution_model = "Fragment";
  entry_point.interfaces = "%output";
  entry_point.execution_modes = "OpExecutionMode %fmain OriginUpperLeft";
  entry_point.body = R"(
%val2 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  generator.add_at_the_end_ = R"(
%foo = OpFunction %void None %func
%foo_entry = OpLabel
%position = OpAccessChain %output_f32vec4_ptr %output %u32_0
OpStore %position %f32vec4_0123
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec allows BuiltIn Position to be used only "
                        "with Vertex, TessellationControl, "
                        "TessellationEvaluation or Geometry execution models"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("called with execution model Fragment"));
}

CodeGenerator GetNoDepthReplacingGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %output_type Block
OpMemberDecorate %output_type 0 BuiltIn FragDepth
)";

  generator.after_types_ = R"(
%output_type = OpTypeStruct %f32
%output_null = OpConstantNull %output_type
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output %output_null
%output_f32_ptr = OpTypePointer Output %f32
)";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Fragment";
  entry_point.interfaces = "%output";
  entry_point.execution_modes = "OpExecutionMode %main OriginUpperLeft";
  entry_point.body = R"(
%val2 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  const std::string function_body = R"(
%foo = OpFunction %void None %func
%foo_entry = OpLabel
%frag_depth = OpAccessChain %output_f32_ptr %output %u32_0
OpStore %frag_depth %f32_1
OpReturn
OpFunctionEnd
)";

  generator.add_at_the_end_ = function_body;

  return generator;
}

TEST_F(ValidateBuiltIns, VulkanFragmentFragDepthNoDepthReplacing) {
  CodeGenerator generator = GetNoDepthReplacingGenerator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec requires DepthReplacing execution mode to "
                        "be declared when using BuiltIn FragDepth"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("VUID-FragDepth-FragDepth-04216"));
}

CodeGenerator GetOneMainHasDepthReplacingOtherHasntGenerator() {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();

  generator.before_types_ = R"(
OpDecorate %output_type Block
OpMemberDecorate %output_type 0 BuiltIn FragDepth
)";

  generator.after_types_ = R"(
%output_type = OpTypeStruct %f32
%output_null = OpConstantNull %output_type
%output_ptr = OpTypePointer Output %output_type
%output = OpVariable %output_ptr Output %output_null
%output_f32_ptr = OpTypePointer Output %f32
)";

  EntryPoint entry_point;
  entry_point.name = "main_d_r";
  entry_point.execution_model = "Fragment";
  entry_point.interfaces = "%output";
  entry_point.execution_modes =
      "OpExecutionMode %main_d_r OriginUpperLeft\n"
      "OpExecutionMode %main_d_r DepthReplacing";
  entry_point.body = R"(
%val2 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  entry_point.name = "main_no_d_r";
  entry_point.execution_model = "Fragment";
  entry_point.interfaces = "%output";
  entry_point.execution_modes = "OpExecutionMode %main_no_d_r OriginUpperLeft";
  entry_point.body = R"(
%val3 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  const std::string function_body = R"(
%foo = OpFunction %void None %func
%foo_entry = OpLabel
%frag_depth = OpAccessChain %output_f32_ptr %output %u32_0
OpStore %frag_depth %f32_1
OpReturn
OpFunctionEnd
)";

  generator.add_at_the_end_ = function_body;

  return generator;
}

TEST_F(ValidateBuiltIns,
       VulkanFragmentFragDepthOneMainHasDepthReplacingOtherHasnt) {
  CodeGenerator generator = GetOneMainHasDepthReplacingOtherHasntGenerator();

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan spec requires DepthReplacing execution mode to "
                        "be declared when using BuiltIn FragDepth"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("VUID-FragDepth-FragDepth-04216"));
}

TEST_F(ValidateBuiltIns, AllowInstanceIdWithIntersectionShader) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.capabilities_ += R"(
OpCapability RayTracingNV
)";

  generator.extensions_ = R"(
OpExtension "SPV_NV_ray_tracing"
)";

  generator.before_types_ = R"(
OpDecorate %input_type Block
OpMemberDecorate %input_type 0 BuiltIn InstanceId
)";

  generator.after_types_ = R"(
%input_type = OpTypeStruct %u32
%input_ptr = OpTypePointer Input %input_type
%input = OpVariable %input_ptr Input
)";

  EntryPoint entry_point;
  entry_point.name = "main_d_r";
  entry_point.execution_model = "IntersectionNV";
  entry_point.interfaces = "%input";
  entry_point.body = R"(
%val2 = OpFunctionCall %void %foo
)";
  generator.entry_points_.push_back(std::move(entry_point));

  generator.add_at_the_end_ = R"(
%foo = OpFunction %void None %func
%foo_entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  EXPECT_THAT(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateBuiltIns, ValidBuiltinsForMeshShader) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.capabilities_ += R"(
OpCapability MeshShadingNV
)";

  generator.extensions_ = R"(
OpExtension "SPV_NV_mesh_shader"
)";

  generator.before_types_ = R"(
OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
OpDecorate %gl_PrimitiveID PerPrimitiveNV
OpDecorate %gl_Layer BuiltIn Layer
OpDecorate %gl_Layer PerPrimitiveNV
OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
OpDecorate %gl_ViewportIndex PerPrimitiveNV
)";

  generator.after_types_ = R"(
%u32_81 = OpConstant %u32 81
%_arr_int_uint_81 = OpTypeArray %i32 %u32_81
%_ptr_Output__arr_int_uint_81 = OpTypePointer Output %_arr_int_uint_81
%gl_PrimitiveID = OpVariable %_ptr_Output__arr_int_uint_81 Output
%gl_Layer = OpVariable %_ptr_Output__arr_int_uint_81 Output
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_int_uint_81 Output
)";

  EntryPoint entry_point;
  entry_point.name = "main_d_r";
  entry_point.execution_model = "MeshNV";
  entry_point.interfaces = "%gl_PrimitiveID %gl_Layer %gl_ViewportIndex";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_1);
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_1));
}

TEST_F(ValidateBuiltIns, InvalidBuiltinsForMeshShader) {
  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.capabilities_ += R"(
OpCapability MeshShadingNV
)";

  generator.extensions_ = R"(
OpExtension "SPV_NV_mesh_shader"
)";

  generator.before_types_ = R"(
OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
OpDecorate %gl_PrimitiveID PerPrimitiveNV
OpDecorate %gl_Layer BuiltIn Layer
OpDecorate %gl_Layer PerPrimitiveNV
OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
OpDecorate %gl_ViewportIndex PerPrimitiveNV
)";

  generator.after_types_ = R"(
%u32_81 = OpConstant %u32 81
%_arr_float_uint_81 = OpTypeArray %f32 %u32_81
%_ptr_Output__arr_float_uint_81 = OpTypePointer Output %_arr_float_uint_81
%gl_PrimitiveID = OpVariable %_ptr_Output__arr_float_uint_81 Output
%gl_Layer = OpVariable %_ptr_Output__arr_float_uint_81 Output
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_float_uint_81 Output
)";

  EntryPoint entry_point;
  entry_point.name = "main_d_r";
  entry_point.execution_model = "MeshNV";
  entry_point.interfaces = "%gl_PrimitiveID %gl_Layer %gl_ViewportIndex";
  entry_point.body = "%ref_load = OpLoad %_arr_float_uint_81 %gl_PrimitiveID";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_1);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("needs to be a 32-bit int scalar"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("is not an int scalar"));
}

TEST_P(ValidateVulkanSubgroupBuiltIns, InMain) {
  const char* const built_in = std::get<0>(GetParam());
  const char* const execution_model = std::get<1>(GetParam());
  const char* const storage_class = std::get<2>(GetParam());
  const char* const data_type = std::get<3>(GetParam());
  const char* const vuid = std::get<4>(GetParam());
  const TestResult& test_result = std::get<5>(GetParam());

  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.capabilities_ += R"(
OpCapability GroupNonUniformBallot
)";

  generator.before_types_ = "OpDecorate %built_in_var BuiltIn ";
  generator.before_types_ += built_in;
  generator.before_types_ += "\n";

  std::ostringstream after_types;
  after_types << "%built_in_ptr = OpTypePointer " << storage_class << " "
              << data_type << "\n";
  after_types << "%built_in_var = OpVariable %built_in_ptr " << storage_class;
  after_types << "\n";
  generator.after_types_ = after_types.str();

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = execution_model;
  if (strncmp(storage_class, "Input", 5) == 0 ||
      strncmp(storage_class, "Output", 6) == 0) {
    entry_point.interfaces = "%built_in_var";
  }
  entry_point.body =
      std::string("%ld = OpLoad ") + data_type + " %built_in_var\n";

  std::ostringstream execution_modes;
  if (0 == std::strcmp(execution_model, "Fragment")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OriginUpperLeft\n";
    if (0 == std::strcmp(built_in, "FragDepth")) {
      execution_modes << "OpExecutionMode %" << entry_point.name
                      << " DepthReplacing\n";
    }
  }
  if (0 == std::strcmp(execution_model, "Geometry")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " InputPoints\n";
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " OutputPoints\n";
  }
  if (0 == std::strcmp(execution_model, "GLCompute")) {
    execution_modes << "OpExecutionMode %" << entry_point.name
                    << " LocalSize 1 1 1\n";
  }
  entry_point.execution_modes = execution_modes.str();

  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_1);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_1));
  if (test_result.error_str) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (test_result.error_str2) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str2));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

INSTANTIATE_TEST_SUITE_P(
    SubgroupMaskNotVec4, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupEqMask", "SubgroupGeMask", "SubgroupGtMask",
                   "SubgroupLeMask", "SubgroupLtMask"),
            Values("GLCompute"), Values("Input"), Values("%u32vec3"),
            Values("VUID-SubgroupEqMask-SubgroupEqMask-04371 "
                   "VUID-SubgroupGeMask-SubgroupGeMask-04373 "
                   "VUID-SubgroupGtMask-SubgroupGtMask-04375 "
                   "VUID-SubgroupLeMask-SubgroupLeMask-04377 "
                   "VUID-SubgroupLtMask-SubgroupLtMask-04379"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit int vector"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupMaskNotU32, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupEqMask", "SubgroupGeMask", "SubgroupGtMask",
                   "SubgroupLeMask", "SubgroupLtMask"),
            Values("GLCompute"), Values("Input"), Values("%f32vec4"),
            Values("VUID-SubgroupEqMask-SubgroupEqMask-04371 "
                   "VUID-SubgroupGeMask-SubgroupGeMask-04373 "
                   "VUID-SubgroupGtMask-SubgroupGtMask-04375 "
                   "VUID-SubgroupLeMask-SubgroupLeMask-04377 "
                   "VUID-SubgroupLtMask-SubgroupLtMask-04379"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 4-component 32-bit int vector"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupMaskNotInput, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupEqMask", "SubgroupGeMask", "SubgroupGtMask",
                   "SubgroupLeMask", "SubgroupLtMask"),
            Values("GLCompute"), Values("Output", "Workgroup", "Private"),
            Values("%u32vec4"),
            Values("VUID-SubgroupEqMask-SubgroupEqMask-04370 "
                   "VUID-SubgroupGeMask-SubgroupGeMask-04372 "
                   "VUID-SubgroupGtMask-SubgroupGtMask-04374 "
                   "VUID-SubgroupLeMask-SubgroupLeMask-04376  "
                   "VUID-SubgroupLtMask-SubgroupLtMask-04378"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(SubgroupMaskOk, ValidateVulkanSubgroupBuiltIns,
                         Combine(Values("SubgroupEqMask", "SubgroupGeMask",
                                        "SubgroupGtMask", "SubgroupLeMask",
                                        "SubgroupLtMask"),
                                 Values("GLCompute"), Values("Input"),
                                 Values("%u32vec4"), Values(nullptr),
                                 Values(TestResult(SPV_SUCCESS, ""))));

TEST_F(ValidateBuiltIns, SubgroupMaskMemberDecorate) {
  const std::string text = R"(
OpCapability Shader
OpCapability GroupNonUniformBallot
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %foo "foo"
OpExecutionMode %foo LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 BuiltIn SubgroupEqMask
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%void_fn = OpTypeFunction %void
%foo = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "BuiltIn SubgroupEqMask cannot be used as a member decoration"));
}

INSTANTIATE_TEST_SUITE_P(
    SubgroupInvocationIdAndSizeNotU32, ValidateVulkanSubgroupBuiltIns,
    Combine(
        Values("SubgroupLocalInvocationId", "SubgroupSize"),
        Values("GLCompute"), Values("Input"), Values("%f32"),
        Values("VUID-SubgroupLocalInvocationId-SubgroupLocalInvocationId-04381 "
               "VUID-SubgroupSize-SubgroupSize-04383"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 32-bit int"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupInvocationIdAndSizeNotInput, ValidateVulkanSubgroupBuiltIns,
    Combine(
        Values("SubgroupLocalInvocationId", "SubgroupSize"),
        Values("GLCompute"), Values("Output", "Workgroup", "Private"),
        Values("%u32"),
        Values("VUID-SubgroupLocalInvocationId-SubgroupLocalInvocationId-04380 "
               "VUID-SubgroupSize-SubgroupSize-04382"),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "to be only used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupInvocationIdAndSizeOk, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupLocalInvocationId", "SubgroupSize"),
            Values("GLCompute"), Values("Input"), Values("%u32"),
            Values(nullptr), Values(TestResult(SPV_SUCCESS, ""))));

TEST_F(ValidateBuiltIns, SubgroupSizeMemberDecorate) {
  const std::string text = R"(
OpCapability Shader
OpCapability GroupNonUniform
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %foo "foo"
OpExecutionMode %foo LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 BuiltIn SubgroupSize
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%void_fn = OpTypeFunction %void
%foo = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("BuiltIn SubgroupSize cannot be used as a member decoration"));
}

INSTANTIATE_TEST_SUITE_P(
    SubgroupNumAndIdNotCompute, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupId", "NumSubgroups"), Values("Vertex"),
            Values("Input"), Values("%u32"),
            Values("VUID-SubgroupId-SubgroupId-04367 "
                   "VUID-NumSubgroups-NumSubgroups-04293"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "to be used only with GLCompute, MeshNV, "
                              "TaskNV, MeshEXT or TaskEXT execution model"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupNumAndIdNotU32, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupId", "NumSubgroups"), Values("GLCompute"),
            Values("Input"), Values("%f32"),
            Values("VUID-SubgroupId-SubgroupId-04369 "
                   "VUID-NumSubgroups-NumSubgroups-04295"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "needs to be a 32-bit int"))));

INSTANTIATE_TEST_SUITE_P(
    SubgroupNumAndIdNotInput, ValidateVulkanSubgroupBuiltIns,
    Combine(Values("SubgroupId", "NumSubgroups"), Values("GLCompute"),
            Values("Output", "Workgroup", "Private"), Values("%u32"),
            Values("VUID-SubgroupId-SubgroupId-04368 "
                   "VUID-NumSubgroups-NumSubgroups-04294"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "to be only used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(SubgroupNumAndIdOk, ValidateVulkanSubgroupBuiltIns,
                         Combine(Values("SubgroupId", "NumSubgroups"),
                                 Values("GLCompute"), Values("Input"),
                                 Values("%u32"), Values(nullptr),
                                 Values(TestResult(SPV_SUCCESS, ""))));

TEST_F(ValidateBuiltIns, SubgroupIdMemberDecorate) {
  const std::string text = R"(
OpCapability Shader
OpCapability GroupNonUniform
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %foo "foo"
OpExecutionMode %foo LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 BuiltIn SubgroupId
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%void_fn = OpTypeFunction %void
%foo = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("BuiltIn SubgroupId cannot be used as a member decoration"));
}

TEST_F(ValidateBuiltIns, TargetIsType) {
  const std::string text = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %void BuiltIn Position
%void = OpTypeVoid
)";

  CompileSuccessfully(text);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("BuiltIns can only target variables, structure members "
                        "or constants"));
}

TEST_F(ValidateBuiltIns, TargetIsVariable) {
  const std::string text = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %wg_var BuiltIn Position
%int = OpTypeInt 32 0
%int_wg_ptr = OpTypePointer Workgroup %int
%wg_var = OpVariable %int_wg_ptr Workgroup
)";

  CompileSuccessfully(text);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

INSTANTIATE_TEST_SUITE_P(
    PrimitiveShadingRateOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("PrimitiveShadingRateKHR"), Values("Vertex", "Geometry"),
            Values("Output"), Values("%u32"),
            Values("OpCapability FragmentShadingRateKHR\n"),
            Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveShadingRateMeshOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("PrimitiveShadingRateKHR"), Values("MeshNV"),
            Values("Output"), Values("%u32"),
            Values("OpCapability FragmentShadingRateKHR\nOpCapability "
                   "MeshShadingNV\n"),
            Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\nOpExtension "
                   "\"SPV_NV_mesh_shader\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveShadingRateInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("PrimitiveShadingRateKHR"), Values("Fragment"), Values("Output"),
        Values("%u32"), Values("OpCapability FragmentShadingRateKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
        Values("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-04484 "),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "Vulkan spec allows BuiltIn PrimitiveShadingRateKHR to be used "
            "only with Vertex, Geometry, MeshNV or MeshEXT execution "
            "models."))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveShadingRateInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("PrimitiveShadingRateKHR"), Values("Vertex"), Values("Input"),
        Values("%u32"), Values("OpCapability FragmentShadingRateKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
        Values("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-04485 "),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "Vulkan spec allows BuiltIn PrimitiveShadingRateKHR to be only "
            "used for variables with Output storage class."))));

INSTANTIATE_TEST_SUITE_P(
    PrimitiveShadingRateInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("PrimitiveShadingRateKHR"), Values("Vertex"), Values("Output"),
        Values("%f32"), Values("OpCapability FragmentShadingRateKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
        Values("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-04486 "),
        Values(TestResult(
            SPV_ERROR_INVALID_DATA,
            "According to the Vulkan spec BuiltIn PrimitiveShadingRateKHR "
            "variable needs to be a 32-bit int scalar."))));

INSTANTIATE_TEST_SUITE_P(
    ShadingRateInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ShadingRateKHR"), Values("Fragment"), Values("Input"),
            Values("%u32"), Values("OpCapability FragmentShadingRateKHR\n"),
            Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    ShadingRateInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ShadingRateKHR"), Values("Vertex"), Values("Input"),
            Values("%u32"), Values("OpCapability FragmentShadingRateKHR\n"),
            Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
            Values("VUID-ShadingRateKHR-ShadingRateKHR-04490 "),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn ShadingRateKHR to be used "
                "only with the Fragment execution model."))));

INSTANTIATE_TEST_SUITE_P(
    ShadingRateInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("ShadingRateKHR"), Values("Fragment"), Values("Output"),
            Values("%u32"), Values("OpCapability FragmentShadingRateKHR\n"),
            Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
            Values("VUID-ShadingRateKHR-ShadingRateKHR-04491 "),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn ShadingRateKHR to be only "
                "used for variables with Input storage class."))));

INSTANTIATE_TEST_SUITE_P(
    ShadingRateInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("ShadingRateKHR"), Values("Fragment"), Values("Input"),
        Values("%f32"), Values("OpCapability FragmentShadingRateKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shading_rate\"\n"),
        Values("VUID-ShadingRateKHR-ShadingRateKHR-04492 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "According to the Vulkan spec BuiltIn ShadingRateKHR "
                          "variable needs to be a 32-bit int scalar."))));

INSTANTIATE_TEST_SUITE_P(
    FragInvocationCountInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragInvocationCountEXT"), Values("Fragment"),
            Values("Input"), Values("%u32"),
            Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FragInvocationCountInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("FragInvocationCountEXT"), Values("Vertex"), Values("Input"),
        Values("%u32"), Values("OpCapability FragmentDensityEXT\n"),
        Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
        Values("VUID-FragInvocationCountEXT-FragInvocationCountEXT-04217"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Vulkan spec allows BuiltIn FragInvocationCountEXT "
                          "to be used only with Fragment execution model."))));

INSTANTIATE_TEST_SUITE_P(
    FragInvocationCountInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragInvocationCountEXT"), Values("Fragment"),
            Values("Output"), Values("%u32"),
            Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values("VUID-FragInvocationCountEXT-FragInvocationCountEXT-04218"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn FragInvocationCountEXT to be only "
                "used for variables with Input storage class."))));

INSTANTIATE_TEST_SUITE_P(
    FragInvocationCountInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragInvocationCountEXT"), Values("Fragment"),
            Values("Input"), Values("%f32"),
            Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values("VUID-FragInvocationCountEXT-FragInvocationCountEXT-04219"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "According to the Vulkan spec BuiltIn FragInvocationCountEXT "
                "variable needs to be a 32-bit int scalar."))));

INSTANTIATE_TEST_SUITE_P(
    FragSizeInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragSizeEXT"), Values("Fragment"), Values("Input"),
            Values("%u32vec2"), Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FragSizeInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragSizeEXT"), Values("Vertex"), Values("Input"),
            Values("%u32vec2"), Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values("VUID-FragSizeEXT-FragSizeEXT-04220"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec allows BuiltIn FragSizeEXT to be "
                              "used only with Fragment execution model."))));

INSTANTIATE_TEST_SUITE_P(
    FragSizeInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("FragSizeEXT"), Values("Fragment"), Values("Output"),
        Values("%u32vec2"), Values("OpCapability FragmentDensityEXT\n"),
        Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
        Values("VUID-FragSizeEXT-FragSizeEXT-04221"),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "Vulkan spec allows BuiltIn FragSizeEXT to be only "
                          "used for variables with Input storage class."))));

INSTANTIATE_TEST_SUITE_P(
    FragSizeInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragSizeEXT"), Values("Fragment"), Values("Input"),
            Values("%u32vec3"), Values("OpCapability FragmentDensityEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_invocation_density\"\n"),
            Values("VUID-FragSizeEXT-FragSizeEXT-04222"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "According to the Vulkan spec BuiltIn FragSizeEXT variable "
                "needs to be a 2-component 32-bit int vector."))));

INSTANTIATE_TEST_SUITE_P(
    FragStencilRefOutputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragStencilRefEXT"), Values("Fragment"), Values("Output"),
            Values("%u32", "%u64"), Values("OpCapability StencilExportEXT\n"),
            Values("OpExtension \"SPV_EXT_shader_stencil_export\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FragStencilRefInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragStencilRefEXT"), Values("Vertex"), Values("Output"),
            Values("%u32", "%u64"), Values("OpCapability StencilExportEXT\n"),
            Values("OpExtension \"SPV_EXT_shader_stencil_export\"\n"),
            Values("VUID-FragStencilRefEXT-FragStencilRefEXT-04223"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec allows BuiltIn FragStencilRefEXT to "
                              "be used only with Fragment execution model."))));

INSTANTIATE_TEST_SUITE_P(
    FragStencilRefInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragStencilRefEXT"), Values("Fragment"), Values("Input"),
            Values("%u32", "%u64"), Values("OpCapability StencilExportEXT\n"),
            Values("OpExtension \"SPV_EXT_shader_stencil_export\"\n"),
            Values("VUID-FragStencilRefEXT-FragStencilRefEXT-04224"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn FragStencilRefEXT to be only used "
                "for variables with Output storage class."))));

INSTANTIATE_TEST_SUITE_P(
    FragStencilRefInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FragStencilRefEXT"), Values("Fragment"), Values("Output"),
            Values("%f32", "%f64", "%u32vec2"),
            Values("OpCapability StencilExportEXT\n"),
            Values("OpExtension \"SPV_EXT_shader_stencil_export\"\n"),
            Values("VUID-FragStencilRefEXT-FragStencilRefEXT-04225"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "According to the Vulkan spec BuiltIn FragStencilRefEXT "
                "variable needs to be a int scalar."))));

INSTANTIATE_TEST_SUITE_P(
    FullyCoveredEXTInputSuccess,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FullyCoveredEXT"), Values("Fragment"), Values("Input"),
            Values("%bool"), Values("OpCapability FragmentFullyCoveredEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_fully_covered\"\n"),
            Values(nullptr), Values(TestResult())));

INSTANTIATE_TEST_SUITE_P(
    FullyCoveredEXTInvalidExecutionModel,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FullyCoveredEXT"), Values("Vertex"), Values("Input"),
            Values("%bool"), Values("OpCapability FragmentFullyCoveredEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_fully_covered\"\n"),
            Values("VUID-FullyCoveredEXT-FullyCoveredEXT-04232"),
            Values(TestResult(SPV_ERROR_INVALID_DATA,
                              "Vulkan spec allows BuiltIn FullyCoveredEXT to "
                              "be used only with Fragment execution model."))));

INSTANTIATE_TEST_SUITE_P(
    FullyCoveredEXTInvalidStorageClass,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FullyCoveredEXT"), Values("Fragment"), Values("Output"),
            Values("%bool"), Values("OpCapability FragmentFullyCoveredEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_fully_covered\"\n"),
            Values("VUID-FullyCoveredEXT-FullyCoveredEXT-04233"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "Vulkan spec allows BuiltIn FullyCoveredEXT to be only used "
                "for variables with Input storage class."))));

INSTANTIATE_TEST_SUITE_P(
    FullyCoveredEXTInvalidType,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("FullyCoveredEXT"), Values("Fragment"), Values("Input"),
            Values("%f32"), Values("OpCapability FragmentFullyCoveredEXT\n"),
            Values("OpExtension \"SPV_EXT_fragment_fully_covered\"\n"),
            Values("VUID-FullyCoveredEXT-FullyCoveredEXT-04234"),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA,
                "According to the Vulkan spec BuiltIn FullyCoveredEXT variable "
                "needs to be a bool scalar."))));

INSTANTIATE_TEST_SUITE_P(
    BaryCoordNotFragment,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("BaryCoordKHR", "BaryCoordNoPerspKHR"), Values("Vertex"),
        Values("Input"), Values("%f32vec3"),
        Values("OpCapability FragmentBarycentricKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"),
        Values("VUID-BaryCoordKHR-BaryCoordKHR-04154 "
               "VUID-BaryCoordNoPerspKHR-BaryCoordNoPerspKHR-04160 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA, "Vulkan spec allows BuiltIn",
                          "to be used only with Fragment execution model"))));

INSTANTIATE_TEST_SUITE_P(
    BaryCoordNotInput,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(Values("BaryCoordKHR", "BaryCoordNoPerspKHR"), Values("Fragment"),
            Values("Output"), Values("%f32vec3"),
            Values("OpCapability FragmentBarycentricKHR\n"),
            Values("OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"),
            Values("VUID-BaryCoordKHR-BaryCoordKHR-04155 "
                   "VUID-BaryCoordNoPerspKHR-BaryCoordNoPerspKHR-04161 "),
            Values(TestResult(
                SPV_ERROR_INVALID_DATA, "Vulkan spec allows BuiltIn",
                "to be only used for variables with Input storage class"))));

INSTANTIATE_TEST_SUITE_P(
    BaryCoordNotFloatVector,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("BaryCoordKHR", "BaryCoordNoPerspKHR"), Values("Fragment"),
        Values("Output"), Values("%f32arr3", "%u32vec4"),
        Values("OpCapability FragmentBarycentricKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"),
        Values("VUID-BaryCoordKHR-BaryCoordKHR-04156 "
               "VUID-BaryCoordNoPerspKHR-BaryCoordNoPerspKHR-04162 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 3-component 32-bit float vector"))));

INSTANTIATE_TEST_SUITE_P(
    BaryCoordNotFloatVec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("BaryCoordKHR", "BaryCoordNoPerspKHR"), Values("Fragment"),
        Values("Output"), Values("%f32vec2"),
        Values("OpCapability FragmentBarycentricKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"),
        Values("VUID-BaryCoordKHR-BaryCoordKHR-04156 "
               "VUID-BaryCoordNoPerspKHR-BaryCoordNoPerspKHR-04162 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 3-component 32-bit float vector"))));

INSTANTIATE_TEST_SUITE_P(
    BaryCoordNotF32Vec3,
    ValidateVulkanCombineBuiltInExecutionModelDataTypeCapabilityExtensionResult,
    Combine(
        Values("BaryCoordKHR", "BaryCoordNoPerspKHR"), Values("Fragment"),
        Values("Output"), Values("%f64vec3"),
        Values("OpCapability FragmentBarycentricKHR\n"),
        Values("OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"),
        Values("VUID-BaryCoordKHR-BaryCoordKHR-04156 "
               "VUID-BaryCoordNoPerspKHR-BaryCoordNoPerspKHR-04162 "),
        Values(TestResult(SPV_ERROR_INVALID_DATA,
                          "needs to be a 3-component 32-bit float vector"))));

std::string GenerateMeshShadingCode(const std::string& built_in,
                                    const std::string& execution_mode,
                                    const std::string& body,
                                    const std::string& declarations = "") {
  std::ostringstream ss;
  ss << R"(
OpCapability MeshShadingEXT
OpExtension "SPV_EXT_mesh_shader"
OpMemoryModel Logical GLSL450
OpEntryPoint MeshEXT %main "main" %var
OpExecutionMode %main LocalSize 1 1 1
OpExecutionMode %main OutputVertices 1
OpExecutionMode %main OutputPrimitivesEXT 16
)";
  ss << "OpExecutionMode %main " << execution_mode << "\n";
  ss << "OpDecorate %var BuiltIn " << built_in << "\n";

  ss << R"(
%void = OpTypeVoid
%func = OpTypeFunction %void
%bool = OpTypeBool
%int = OpTypeInt 32 1
%uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%v3uint = OpTypeVector %uint 3

%int_0 = OpConstant %int 0
%uint_16 = OpConstant %uint 16
)";

  ss << declarations;

  ss << R"(
%main = OpFunction %void None %func
%main_entry = OpLabel
)";

  ss << body;

  ss << R"(
OpReturn
OpFunctionEnd)";
  return ss.str();
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveTriangleIndicesEXTSuccess) {
  const std::string declarations = R"(
%array = OpTypeArray %v3uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %v3uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveTriangleIndicesEXT",
                              "OutputTrianglesEXT", body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns,
       VulkanPrimitiveTriangleIndicesEXTInvalidExecutionMode) {
  const std::string declarations = R"(
%array = OpTypeArray %v3uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %v3uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveTriangleIndicesEXT", "OutputPoints",
                              body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07054"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveTriangleIndicesEXTStorageClass) {
  const std::string declarations = R"(
%array = OpTypeArray %v3uint %uint_16
%array_ptr = OpTypePointer Input %array
%var = OpVariable %array_ptr Input
%ptr = OpTypePointer Input %v3uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveTriangleIndicesEXT",
                              "OutputTrianglesEXT", body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07055"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveTriangleIndicesEXTVectorSize) {
  const std::string declarations = R"(
%array = OpTypeArray %v2uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %v2uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveTriangleIndicesEXT",
                              "OutputTrianglesEXT", body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07056"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveTriangleIndicesEXTNonArray) {
  const std::string declarations = R"(
%ptr = OpTypePointer Output %v3uint
%var = OpVariable %ptr Output
)";
  const std::string body = R"(
%load = OpLoad %v3uint %var
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveTriangleIndicesEXT",
                              "OutputTrianglesEXT", body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07056"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveLineIndicesEXTSuccess) {
  const std::string declarations = R"(
%array = OpTypeArray %v2uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %v2uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveLineIndicesEXT", "OutputLinesEXT", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveLineIndicesEXTInvalidExecutionMode) {
  const std::string declarations = R"(
  %array = OpTypeArray %v2uint %uint_16
  %array_ptr = OpTypePointer Output %array
  %var = OpVariable %array_ptr Output
  %ptr = OpTypePointer Output %v2uint
  )";
  const std::string body = R"(
  %access = OpAccessChain %ptr %var %int_0
  )";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveLineIndicesEXT", "OutputPoints", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveLineIndicesEXT-PrimitiveLineIndicesEXT-07048"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveLineIndicesEXTStorageClass) {
  const std::string declarations = R"(
%array = OpTypeArray %v2uint %uint_16
%array_ptr = OpTypePointer Input %array
%var = OpVariable %array_ptr Input
%ptr = OpTypePointer Input %v2uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveLineIndicesEXT", "OutputLinesEXT", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveLineIndicesEXT-PrimitiveLineIndicesEXT-07049"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitiveLineIndicesEXTType) {
  const std::string declarations = R"(
%array = OpTypeArray %v3uint %uint_16
%array_ptr = OpTypePointer Input %array
%var = OpVariable %array_ptr Input
%ptr = OpTypePointer Input %v3uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitiveLineIndicesEXT", "OutputLinesEXT", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveLineIndicesEXT-PrimitiveLineIndicesEXT-07050"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitivePointIndicesEXTSuccess) {
  const std::string declarations = R"(
%array = OpTypeArray %uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitivePointIndicesEXT", "OutputPoints", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns, VulkanPrimitivePointIndicesEXTInvalidExecutionMode) {
  const std::string declarations = R"(
    %array = OpTypeArray %uint %uint_16
    %array_ptr = OpTypePointer Output %array
    %var = OpVariable %array_ptr Output
    %ptr = OpTypePointer Output %uint
    )";
  const std::string body = R"(
    %access = OpAccessChain %ptr %var %int_0
    )";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitivePointIndicesEXT", "OutputTrianglesNV",
                              body, declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07042"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitivePointIndicesEXTStorageClass) {
  const std::string declarations = R"(
%array = OpTypeArray %uint %uint_16
%array_ptr = OpTypePointer Input %array
%var = OpVariable %array_ptr Input
%ptr = OpTypePointer Input %uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitivePointIndicesEXT", "OutputPoints", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07043"));
}

TEST_F(ValidateBuiltIns, VulkanPrimitivePointIndicesEXTType) {
  const std::string declarations = R"(
%array = OpTypeArray %v3uint %uint_16
%array_ptr = OpTypePointer Output %array
%var = OpVariable %array_ptr Output
%ptr = OpTypePointer Output %v3uint
)";
  const std::string body = R"(
%access = OpAccessChain %ptr %var %int_0
)";

  CompileSuccessfully(
      GenerateMeshShadingCode("PrimitivePointIndicesEXT", "OutputPoints", body,
                              declarations)
          .c_str(),
      SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07044"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinPrimtiveIDWithPerPrimitiveEXT) {
  const std::string text = R"(
               OpCapability MeshShadingEXT
               OpCapability Shader
               OpExtension "SPV_EXT_mesh_shader"
               OpMemoryModel Logical GLSL450
               OpEntryPoint MeshEXT %MainMesh "MainMesh" %gl_PrimitiveID
               OpExecutionMode %MainMesh OutputPrimitivesNV 1
               OpExecutionMode %MainMesh OutputVertices 3
               OpExecutionMode %MainMesh OutputTrianglesNV
               OpExecutionMode %MainMesh LocalSize 1 1 1
               OpSource Slang 1
               OpName %MainMesh "MainMesh"
               OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
               OpDecorate %gl_PrimitiveID PerPrimitiveNV
       %void = OpTypeVoid
          %9 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
     %uint_1 = OpConstant %uint 1
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
      %int_1 = OpConstant %int 1
      %int_3 = OpConstant %int 3
%_ptr_Output_v4float = OpTypePointer Output %v4float
     %uint_0 = OpConstant %uint 0
    %v3float = OpTypeVector %float 3
%_ptr_Output_v3float = OpTypePointer Output %v3float
     %v3uint = OpTypeVector %uint 3
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%_ptr_Output_int = OpTypePointer Output %int
%_arr_int_int_1 = OpTypeArray %int %int_1
%_ptr_Output__arr_int_int_1 = OpTypePointer Output %_arr_int_int_1
%gl_PrimitiveID = OpVariable %_ptr_Output__arr_int_int_1 Output
   %MainMesh = OpFunction %void None %9
         %25 = OpLabel
               OpSetMeshOutputsEXT %uint_3 %uint_1
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinPrimtiveIDWithPerPrimitiveEXT) {
  const std::string text = R"(
       OpCapability MeshShadingEXT
       OpCapability Shader
       OpExtension "SPV_EXT_mesh_shader"
       OpMemoryModel Logical GLSL450
       OpEntryPoint MeshEXT %MainMesh "MainMesh" %gl_PrimitiveID
       OpExecutionMode %MainMesh OutputPrimitivesNV 1
       OpExecutionMode %MainMesh OutputVertices 3
       OpExecutionMode %MainMesh OutputTrianglesNV
       OpExecutionMode %MainMesh LocalSize 1 1 1
       OpSource Slang 1
       OpName %MainMesh "MainMesh"
       OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
%void = OpTypeVoid
  %9 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_3 = OpConstant %uint 3
%uint_1 = OpConstant %uint 1
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%int = OpTypeInt 32 1
%int_1 = OpConstant %int 1
%int_3 = OpConstant %int 3
%_ptr_Output_v4float = OpTypePointer Output %v4float
%uint_0 = OpConstant %uint 0
%v3float = OpTypeVector %float 3
%_ptr_Output_v3float = OpTypePointer Output %v3float
%v3uint = OpTypeVector %uint 3
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%_ptr_Output_int = OpTypePointer Output %int
%_arr_int_int_1 = OpTypeArray %int %int_1
%_ptr_Output__arr_int_int_1 = OpTypePointer Output %_arr_int_int_1
%gl_PrimitiveID = OpVariable %_ptr_Output__arr_int_int_1 Output
%MainMesh = OpFunction %void None %9
 %25 = OpLabel
%ref_load = OpLoad %_arr_int_int_1 %gl_PrimitiveID
       OpSetMeshOutputsEXT %uint_3 %uint_1
       OpReturn
       OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveId-PrimitiveId-07040"));
}


TEST_F(ValidateBuiltIns, BadVulkanBuiltinViewportIndexWithPerPrimitiveEXT) {
  const std::string text = R"(
     OpCapability MeshShadingEXT
     OpCapability Shader
     OpExtension "SPV_EXT_mesh_shader"
     OpMemoryModel Logical GLSL450
     OpEntryPoint MeshEXT %MainMesh "MainMesh" %gl_ViewportIndex
     OpExecutionMode %MainMesh OutputPrimitivesNV 1
     OpExecutionMode %MainMesh OutputVertices 3
     OpExecutionMode %MainMesh OutputTrianglesNV
     OpExecutionMode %MainMesh LocalSize 1 1 1
     OpSource Slang 1
     OpName %MainMesh "MainMesh"
     OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
%void = OpTypeVoid
%9 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_3 = OpConstant %uint 3
%uint_1 = OpConstant %uint 1
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%int_1 = OpConstant %int 1
%int_3 = OpConstant %int 3
%uint_0 = OpConstant %uint 0
%v3float = OpTypeVector %float 3
%_ptr_Output_v3float = OpTypePointer Output %v3float
%v3uint = OpTypeVector %uint 3
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%_ptr_Output_int = OpTypePointer Output %int
%_arr_int_int_1 = OpTypeArray %int %int_1
%_ptr_Output__arr_int_int_1 = OpTypePointer Output %_arr_int_int_1
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_int_int_1 Output
%MainMesh = OpFunction %void None %9
%25 = OpLabel
%ref_load = OpLoad %_arr_int_int_1 %gl_ViewportIndex
     OpSetMeshOutputsEXT %uint_3 %uint_1
     OpReturn
     OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-ViewportIndex-ViewportIndex-07060"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinPrimitivePointIndicesEXT) {
  const std::string text = R"(
       OpCapability MeshShadingEXT
       OpExtension "SPV_EXT_mesh_shader"
  %1 = OpExtInstImport "GLSL.std.450"
       OpMemoryModel Logical GLSL450
       OpEntryPoint MeshEXT %main "main" %gl_PrimitivePointIndicesEXT
       OpExecutionMode %main LocalSize 32 1 1
       OpExecutionMode %main OutputVertices 81
       OpExecutionMode %main OutputPrimitivesEXT 32
       OpExecutionMode %main OutputPoints
       OpSource GLSL 460
       OpSourceExtension "GL_EXT_mesh_shader"
       OpName %main "main"
       OpName %gl_PrimitivePointIndicesEXT "gl_PrimitivePointIndicesEXT"
       OpDecorate %gl_PrimitivePointIndicesEXT BuiltIn PrimitivePointIndicesEXT
       OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
%void = OpTypeVoid
  %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%_arr_uint_uint_32 = OpTypeArray %uint %uint_32
%_ptr_Output__arr_uint_uint_32 = OpTypePointer Output %_arr_uint_uint_32
%gl_PrimitivePointIndicesEXT = OpVariable %_ptr_Output__arr_uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%_ptr_Output_uint = OpTypePointer Output %uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %3
  %5 = OpLabel
 %15 = OpAccessChain %_ptr_Output_uint %gl_PrimitivePointIndicesEXT %int_0
       OpStore %15 %uint_0
       OpReturn
       OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinPrimitiveLineIndicesEXT) {
  const std::string text = R"(
          OpCapability MeshShadingEXT
          OpExtension "SPV_EXT_mesh_shader"
     %1 = OpExtInstImport "GLSL.std.450"
          OpMemoryModel Logical GLSL450
          OpEntryPoint MeshEXT %main "main" %gl_PrimitiveLineIndicesEXT
          OpExecutionMode %main LocalSize 32 1 1
          OpExecutionMode %main OutputVertices 81
          OpExecutionMode %main OutputPrimitivesEXT 32
          OpExecutionMode %main OutputLinesEXT
          OpSource GLSL 460
          OpSourceExtension "GL_EXT_mesh_shader"
          OpName %main "main"
          OpName %gl_PrimitiveLineIndicesEXT "gl_PrimitiveLineIndicesEXT"
          OpDecorate %gl_PrimitiveLineIndicesEXT BuiltIn PrimitiveLineIndicesEXT
          OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
  %void = OpTypeVoid
     %3 = OpTypeFunction %void
  %uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%uint_32 = OpConstant %uint 32
%_arr_v2uint_uint_32 = OpTypeArray %v2uint %uint_32
%_ptr_Output__arr_v2uint_uint_32 = OpTypePointer Output %_arr_v2uint_uint_32
%gl_PrimitiveLineIndicesEXT = OpVariable %_ptr_Output__arr_v2uint_uint_32 Output
   %int = OpTypeInt 32 1
 %int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
    %15 = OpConstantComposite %v2uint %uint_0 %uint_0
%_ptr_Output_v2uint = OpTypePointer Output %v2uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
  %main = OpFunction %void None %3
     %5 = OpLabel
    %17 = OpAccessChain %_ptr_Output_v2uint %gl_PrimitiveLineIndicesEXT %int_0
          OpStore %17 %15
          OpReturn
          OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinPrimitiveLineIndicesEXT) {
  const std::string text = R"(
          OpCapability MeshShadingEXT
          OpExtension "SPV_EXT_mesh_shader"
     %1 = OpExtInstImport "GLSL.std.450"
          OpMemoryModel Logical GLSL450
          OpEntryPoint MeshEXT %main "main" %gl_PrimitiveLineIndicesEXT
          OpExecutionMode %main LocalSize 32 1 1
          OpExecutionMode %main OutputVertices 81
          OpExecutionMode %main OutputPrimitivesEXT 32
          OpExecutionMode %main OutputPoints
          OpSource GLSL 460
          OpSourceExtension "GL_EXT_mesh_shader"
          OpName %main "main"
          OpName %gl_PrimitiveLineIndicesEXT "gl_PrimitiveLineIndicesEXT"
          OpDecorate %gl_PrimitiveLineIndicesEXT BuiltIn PrimitiveLineIndicesEXT
          OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
  %void = OpTypeVoid
     %3 = OpTypeFunction %void
  %uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%uint_32 = OpConstant %uint 32
%_arr_v2uint_uint_32 = OpTypeArray %v2uint %uint_32
%_ptr_Output__arr_v2uint_uint_32 = OpTypePointer Output %_arr_v2uint_uint_32
%gl_PrimitiveLineIndicesEXT = OpVariable %_ptr_Output__arr_v2uint_uint_32 Output
   %int = OpTypeInt 32 1
 %int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
    %15 = OpConstantComposite %v2uint %uint_0 %uint_0
%_ptr_Output_v2uint = OpTypePointer Output %v2uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
  %main = OpFunction %void None %3
     %5 = OpLabel
    %17 = OpAccessChain %_ptr_Output_v2uint %gl_PrimitiveLineIndicesEXT %int_0
          OpStore %17 %15
          OpReturn
          OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveLineIndicesEXT-PrimitiveLineIndicesEXT-07048"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinPrimitivePointIndicesEXT) {
  const std::string text = R"(
       OpCapability MeshShadingEXT
       OpExtension "SPV_EXT_mesh_shader"
  %1 = OpExtInstImport "GLSL.std.450"
       OpMemoryModel Logical GLSL450
       OpEntryPoint MeshEXT %main "main" %gl_PrimitivePointIndicesEXT
       OpExecutionMode %main LocalSize 32 1 1
       OpExecutionMode %main OutputVertices 81
       OpExecutionMode %main OutputPrimitivesEXT 32
       OpExecutionMode %main OutputTrianglesEXT
       OpSource GLSL 460
       OpSourceExtension "GL_EXT_mesh_shader"
       OpName %main "main"
       OpName %gl_PrimitivePointIndicesEXT "gl_PrimitivePointIndicesEXT"
       OpDecorate %gl_PrimitivePointIndicesEXT BuiltIn PrimitivePointIndicesEXT
       OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
%void = OpTypeVoid
  %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%_arr_uint_uint_32 = OpTypeArray %uint %uint_32
%_ptr_Output__arr_uint_uint_32 = OpTypePointer Output %_arr_uint_uint_32
%gl_PrimitivePointIndicesEXT = OpVariable %_ptr_Output__arr_uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%_ptr_Output_uint = OpTypePointer Output %uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %3
  %5 = OpLabel
 %15 = OpAccessChain %_ptr_Output_uint %gl_PrimitivePointIndicesEXT %int_0
       OpStore %15 %uint_0
       OpReturn
       OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07042"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinPrimitiveTriangleIndicesEXT) {
  const std::string text = R"(
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_PrimitiveTriangleIndicesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 460
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_PrimitiveTriangleIndicesEXT "gl_PrimitiveTriangleIndicesEXT"
    OpDecorate %gl_PrimitiveTriangleIndicesEXT BuiltIn PrimitiveTriangleIndicesEXT
%void = OpTypeVoid
%7 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_32 = OpTypeArray %v3uint %uint_32
%_ptr_Output__arr_v3uint_uint_32 = OpTypePointer Output %_arr_v3uint_uint_32
%gl_PrimitiveTriangleIndicesEXT = OpVariable %_ptr_Output__arr_v3uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%15 = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%17 = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %7
%18 = OpLabel
%19 = OpAccessChain %_ptr_Output_v3uint %gl_PrimitiveTriangleIndicesEXT %int_0
    OpStore %19 %15
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinPrimitiveTriangleIndicesEXT) {
  const std::string text = R"(
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_PrimitiveTriangleIndicesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputPoints
    OpSource GLSL 460
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_PrimitiveTriangleIndicesEXT "gl_PrimitiveTriangleIndicesEXT"
    OpDecorate %gl_PrimitiveTriangleIndicesEXT BuiltIn PrimitiveTriangleIndicesEXT
%void = OpTypeVoid
%7 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_32 = OpTypeArray %v3uint %uint_32
%_ptr_Output__arr_v3uint_uint_32 = OpTypePointer Output %_arr_v3uint_uint_32
%gl_PrimitiveTriangleIndicesEXT = OpVariable %_ptr_Output__arr_v3uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%15 = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%17 = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %7
%18 = OpLabel
%19 = OpAccessChain %_ptr_Output_v3uint %gl_PrimitiveTriangleIndicesEXT %int_0
    OpStore %19 %15
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07054"));
}

TEST_F(ValidateBuiltIns, BadVulkanPrimitivePointIndicesArraySizeMeshEXT) {
  const std::string text = R"(
   OpCapability MeshShadingEXT
   OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
   OpMemoryModel Logical GLSL450
   OpEntryPoint MeshEXT %main "main" %gl_PrimitivePointIndicesEXT
   OpExecutionMode %main LocalSize 32 1 1
   OpExecutionMode %main OutputVertices 81
   OpExecutionMode %main OutputPrimitivesEXT 16
   OpExecutionMode %main OutputPoints
   OpSource GLSL 460
   OpSourceExtension "GL_EXT_mesh_shader"
   OpName %main "main"
   OpName %gl_PrimitivePointIndicesEXT "gl_PrimitivePointIndicesEXT"
   OpDecorate %gl_PrimitivePointIndicesEXT BuiltIn PrimitivePointIndicesEXT
   OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%_arr_uint_uint_32 = OpTypeArray %uint %uint_32
%_ptr_Output__arr_uint_uint_32 = OpTypePointer Output %_arr_uint_uint_32
%gl_PrimitivePointIndicesEXT = OpVariable %_ptr_Output__arr_uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%_ptr_Output_uint = OpTypePointer Output %uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %3
%5 = OpLabel
%15 = OpAccessChain %_ptr_Output_uint %gl_PrimitivePointIndicesEXT %int_0
   OpStore %15 %uint_0
   OpReturn
   OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07046"));
}

TEST_F(ValidateBuiltIns, BadVulkanPrimitiveLineIndicesArraySizeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_PrimitiveLineIndicesEXT
      OpExecutionMode %main LocalSize 32 1 1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 16
      OpExecutionMode %main OutputLinesEXT
      OpSource GLSL 460
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_PrimitiveLineIndicesEXT "gl_PrimitiveLineIndicesEXT"
      OpDecorate %gl_PrimitiveLineIndicesEXT BuiltIn PrimitiveLineIndicesEXT
      OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%uint_32 = OpConstant %uint 32
%_arr_v2uint_uint_32 = OpTypeArray %v2uint %uint_32
%_ptr_Output__arr_v2uint_uint_32 = OpTypePointer Output %_arr_v2uint_uint_32
%gl_PrimitiveLineIndicesEXT = OpVariable %_ptr_Output__arr_v2uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%15 = OpConstantComposite %v2uint %uint_0 %uint_0
%_ptr_Output_v2uint = OpTypePointer Output %v2uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %3
 %5 = OpLabel
%17 = OpAccessChain %_ptr_Output_v2uint %gl_PrimitiveLineIndicesEXT %int_0
      OpStore %17 %15
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveLineIndicesEXT-PrimitiveLineIndicesEXT-07052"));
}

TEST_F(ValidateBuiltIns, BadVulkanPrimitiveTriangleIndicesArraySizeMeshEXT) {
  const std::string text = R"(
  OpCapability MeshShadingEXT
  OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
  OpMemoryModel Logical GLSL450
  OpEntryPoint MeshEXT %main "main" %gl_PrimitiveTriangleIndicesEXT
  OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
  OpExecutionMode %main OutputVertices 81
  OpExecutionMode %main OutputPrimitivesEXT 16
  OpExecutionMode %main OutputTrianglesEXT
  OpSource GLSL 460
  OpSourceExtension "GL_EXT_mesh_shader"
  OpName %main "main"
  OpName %gl_PrimitiveTriangleIndicesEXT "gl_PrimitiveTriangleIndicesEXT"
  OpDecorate %gl_PrimitiveTriangleIndicesEXT BuiltIn PrimitiveTriangleIndicesEXT
%void = OpTypeVoid
%7 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_32 = OpTypeArray %v3uint %uint_32
%_ptr_Output__arr_v3uint_uint_32 = OpTypePointer Output %_arr_v3uint_uint_32
%gl_PrimitiveTriangleIndicesEXT = OpVariable %_ptr_Output__arr_v3uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%15 = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%17 = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %7
%18 = OpLabel
%19 = OpAccessChain %_ptr_Output_v3uint %gl_PrimitiveTriangleIndicesEXT %int_0
  OpStore %19 %15
  OpReturn
  OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveTriangleIndicesEXT-"
                      "PrimitiveTriangleIndicesEXT-07058"));
}

TEST_F(ValidateBuiltIns, BadExecModelVulkanPrimitivePointIndicesEXT) {
  const std::string text = R"(
  OpCapability MeshShadingNV
  OpCapability MeshShadingEXT
  OpExtension "SPV_NV_mesh_shader"
  OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
  OpMemoryModel Logical GLSL450
  OpEntryPoint MeshNV %main "main" %gl_PrimitivePointIndicesEXT
  OpExecutionMode %main LocalSize 32 1 1
  OpExecutionMode %main OutputVertices 81
  OpExecutionMode %main OutputPrimitivesEXT 32
  OpExecutionMode %main OutputPoints
  OpSource GLSL 460
  OpSourceExtension "GL_EXT_mesh_shader"
  OpName %main "main"
  OpName %gl_PrimitivePointIndicesEXT "gl_PrimitivePointIndicesEXT"
  OpDecorate %gl_PrimitivePointIndicesEXT BuiltIn PrimitivePointIndicesEXT
  OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%_arr_uint_uint_32 = OpTypeArray %uint %uint_32
%_ptr_Output__arr_uint_uint_32 = OpTypePointer Output %_arr_uint_uint_32
%gl_PrimitivePointIndicesEXT = OpVariable %_ptr_Output__arr_uint_uint_32 Output
%int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%_ptr_Output_uint = OpTypePointer Output %uint
%v3uint = OpTypeVector %uint 3
%uint_1 = OpConstant %uint 1
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_32 %uint_1 %uint_1
%main = OpFunction %void None %3
%5 = OpLabel
%15 = OpAccessChain %_ptr_Output_uint %gl_PrimitivePointIndicesEXT %int_0
  OpStore %15 %uint_0
  OpReturn
  OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitivePointIndicesEXT-PrimitivePointIndicesEXT-07041"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinCullPrimitiveEXTInBlock) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTBlockArraySize) {
  const std::string text = R"(
        OpCapability MeshShadingEXT
        OpExtension "SPV_EXT_mesh_shader"
   %1 = OpExtInstImport "GLSL.std.450"
        OpMemoryModel Logical GLSL450
        OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
        OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
        OpExecutionMode %main OutputVertices 81
        OpExecutionMode %main OutputPrimitivesEXT 32
        OpExecutionMode %main OutputTrianglesEXT
        OpSource GLSL 450
        OpSourceExtension "GL_EXT_mesh_shader"
        OpName %main "main"
        OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
        OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
        OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
        OpDecorate %gl_MeshPerPrimitiveEXT Block
        OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
        OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
  %void = OpTypeVoid
   %3 = OpTypeFunction %void
  %uint = OpTypeInt 32 0
  %uint_32 = OpConstant %uint 32
  %uint_16 = OpConstant %uint 16
  %uint_1 = OpConstant %uint 1
  %int = OpTypeInt 32 1
  %bool = OpTypeBool
  %gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
  %_arr_gl_MeshPerPrimitiveEXT_uint_16 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_16
  %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_16 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_16
  %gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_16 Output
  %main = OpFunction %void None %3
   %5 = OpLabel
 %ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_16 %gl_MeshPrimitivesEXT
        OpReturn
        OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-10590"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinCullPrimitiveEXTMissingBlock) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07036"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("Scalar boolean must be in a Block"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTType) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07036"));
}

// from https://github.com/KhronosGroup/SPIRV-Tools/issues/5980
TEST_F(ValidateBuiltIns, VulkanBuiltinCullPrimitiveEXTArrayOfBool) {
  const std::string text = R"(
               OpCapability MeshShadingEXT
               OpExtension "SPV_EXT_mesh_shader"
               OpMemoryModel Logical GLSL450
               OpEntryPoint MeshEXT %main "main" %gl_LocalInvocationIndex %gl_Position %4 %5
               OpExecutionMode %main LocalSize 2 1 1
               OpExecutionMode %main OutputTrianglesEXT
               OpExecutionMode %main OutputVertices 2
               OpExecutionMode %main OutputPrimitivesEXT 2
               OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex
               OpDecorate %gl_Position BuiltIn Position
               OpDecorate %4 BuiltIn PrimitiveTriangleIndicesEXT
               OpDecorate %5 BuiltIn CullPrimitiveEXT
               OpDecorate %5 PerPrimitiveEXT
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
       %bool = OpTypeBool
      %false = OpConstantFalse %bool
%_ptr_Input_uint = OpTypePointer Input %uint
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_arr_v4float_uint_2 = OpTypeArray %v4float %uint_2
%_ptr_Output__arr_v4float_uint_2 = OpTypePointer Output %_arr_v4float_uint_2
     %v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_ptr_Output__arr_v3uint_uint_2 = OpTypePointer Output %_arr_v3uint_uint_2
%_arr_bool_uint_2 = OpTypeArray %bool %uint_2
%_ptr_Output__arr_bool_uint_2 = OpTypePointer Output %_arr_bool_uint_2
       %void = OpTypeVoid
         %21 = OpTypeFunction %void
%_ptr_Output_bool = OpTypePointer Output %bool
%gl_LocalInvocationIndex = OpVariable %_ptr_Input_uint Input
%gl_Position = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
          %4 = OpVariable %_ptr_Output__arr_v3uint_uint_2 Output
          %5 = OpVariable %_ptr_Output__arr_bool_uint_2 Output
       %main = OpFunction %void None %21
         %23 = OpLabel
         %24 = OpLoad %uint %gl_LocalInvocationIndex
               OpSetMeshOutputsEXT %uint_2 %uint_2
         %25 = OpAccessChain %_ptr_Output_bool %5 %24
               OpStore %25 %false
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTArrayType) {
  const std::string text = R"(
               OpCapability MeshShadingEXT
               OpExtension "SPV_EXT_mesh_shader"
               OpMemoryModel Logical GLSL450
               OpEntryPoint MeshEXT %main "main" %gl_LocalInvocationIndex %gl_Position %4 %5
               OpExecutionMode %main LocalSize 2 1 1
               OpExecutionMode %main OutputTrianglesEXT
               OpExecutionMode %main OutputVertices 2
               OpExecutionMode %main OutputPrimitivesEXT 2
               OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex
               OpDecorate %gl_Position BuiltIn Position
               OpDecorate %4 BuiltIn PrimitiveTriangleIndicesEXT
               OpDecorate %5 BuiltIn CullPrimitiveEXT
               OpDecorate %5 PerPrimitiveEXT
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
       %bool = OpTypeBool
      %false = OpConstantFalse %bool
%_ptr_Input_uint = OpTypePointer Input %uint
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_arr_v4float_uint_2 = OpTypeArray %v4float %uint_2
%_ptr_Output__arr_v4float_uint_2 = OpTypePointer Output %_arr_v4float_uint_2
     %v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_ptr_Output__arr_v3uint_uint_2 = OpTypePointer Output %_arr_v3uint_uint_2
%_arr_uint_uint_2 = OpTypeArray %uint %uint_2
%_ptr_Output__arr_uint_uint_2 = OpTypePointer Output %_arr_uint_uint_2
       %void = OpTypeVoid
         %21 = OpTypeFunction %void
%_ptr_Output_uint = OpTypePointer Output %uint
%gl_LocalInvocationIndex = OpVariable %_ptr_Input_uint Input
%gl_Position = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
          %4 = OpVariable %_ptr_Output__arr_v3uint_uint_2 Output
          %5 = OpVariable %_ptr_Output__arr_uint_uint_2 Output
       %main = OpFunction %void None %21
         %23 = OpLabel
         %24 = OpLoad %uint %gl_LocalInvocationIndex
               OpSetMeshOutputsEXT %uint_2 %uint_2
         %25 = OpAccessChain %_ptr_Output_uint %5 %24
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07036"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTArrayOfBoolSize) {
  const std::string text = R"(
          OpCapability MeshShadingEXT
          OpExtension "SPV_EXT_mesh_shader"
          OpMemoryModel Logical GLSL450
          OpEntryPoint MeshEXT %main "main" %gl_LocalInvocationIndex %gl_Position %4 %5
          OpExecutionMode %main LocalSize 2 1 1
          OpExecutionMode %main OutputTrianglesEXT
          OpExecutionMode %main OutputVertices 2
          OpExecutionMode %main OutputPrimitivesEXT 2
          OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex
          OpDecorate %gl_Position BuiltIn Position
          OpDecorate %4 BuiltIn PrimitiveTriangleIndicesEXT
          OpDecorate %5 BuiltIn CullPrimitiveEXT
          OpDecorate %5 PerPrimitiveEXT
  %uint = OpTypeInt 32 0
%uint_2 = OpConstant %uint 2
%uint_4 = OpConstant %uint 4
  %bool = OpTypeBool
 %false = OpConstantFalse %bool
%_ptr_Input_uint = OpTypePointer Input %uint
 %float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_arr_v4float_uint_2 = OpTypeArray %v4float %uint_2
%_ptr_Output__arr_v4float_uint_2 = OpTypePointer Output %_arr_v4float_uint_2
%v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_ptr_Output__arr_v3uint_uint_2 = OpTypePointer Output %_arr_v3uint_uint_2
%_arr_bool_uint_4 = OpTypeArray %bool %uint_4
%_ptr_Output__arr_bool_uint_4 = OpTypePointer Output %_arr_bool_uint_4
  %void = OpTypeVoid
    %21 = OpTypeFunction %void
%_ptr_Output_bool = OpTypePointer Output %bool
%gl_LocalInvocationIndex = OpVariable %_ptr_Input_uint Input
%gl_Position = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
     %4 = OpVariable %_ptr_Output__arr_v3uint_uint_2 Output
     %5 = OpVariable %_ptr_Output__arr_bool_uint_4 Output
  %main = OpFunction %void None %21
    %23 = OpLabel
    %24 = OpLoad %uint %gl_LocalInvocationIndex
          OpSetMeshOutputsEXT %uint_2 %uint_2
    %25 = OpAccessChain %_ptr_Output_bool %5 %24
          OpStore %25 %false
          OpReturn
          OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-10589"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTInterfaceVariable) {
  const std::string text = R"(
         OpCapability MeshShadingEXT
         OpExtension "SPV_EXT_mesh_shader"
         OpMemoryModel Logical GLSL450
         OpEntryPoint MeshEXT %main "main" %gl_LocalInvocationIndex %gl_Position %4 %5 %gl_MeshPrimitivesEXT
         OpExecutionMode %main LocalSize 2 1 1
         OpExecutionMode %main OutputTrianglesEXT
         OpExecutionMode %main OutputVertices 2
         OpExecutionMode %main OutputPrimitivesEXT 2
         OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex
         OpDecorate %gl_Position BuiltIn Position
         OpDecorate %4 BuiltIn PrimitiveTriangleIndicesEXT
         OpDecorate %5 BuiltIn CullPrimitiveEXT
         OpDecorate %5 PerPrimitiveEXT
         OpDecorate %gl_MeshPerPrimitiveEXT Block
         OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
         OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
 %uint = OpTypeInt 32 0
%uint_2 = OpConstant %uint 2
 %bool = OpTypeBool
%false = OpConstantFalse %bool
%_ptr_Input_uint = OpTypePointer Input %uint
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_arr_v4float_uint_2 = OpTypeArray %v4float %uint_2
%_ptr_Output__arr_v4float_uint_2 = OpTypePointer Output %_arr_v4float_uint_2
%v3uint = OpTypeVector %uint 3
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_ptr_Output__arr_v3uint_uint_2 = OpTypePointer Output %_arr_v3uint_uint_2
%_arr_bool_uint_2 = OpTypeArray %bool %uint_2
%_ptr_Output__arr_bool_uint_2 = OpTypePointer Output %_arr_bool_uint_2
 %void = OpTypeVoid
   %21 = OpTypeFunction %void
%_ptr_Output_bool = OpTypePointer Output %bool
%gl_LocalInvocationIndex = OpVariable %_ptr_Input_uint Input
%gl_Position = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
    %4 = OpVariable %_ptr_Output__arr_v3uint_uint_2 Output
    %5 = OpVariable %_ptr_Output__arr_bool_uint_2 Output
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_2 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_2
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_2 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_2
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_2 Output
 %main = OpFunction %void None %21
   %23 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_2 %gl_MeshPrimitivesEXT
   %24 = OpLoad %uint %gl_LocalInvocationIndex
         OpSetMeshOutputsEXT %uint_2 %uint_2
   %25 = OpAccessChain %_ptr_Output_bool %5 %24
         OpStore %25 %false
         OpReturn
         OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-10591"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinCullPrimitiveEXTStorageClass) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Input %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Input
%main = OpFunction %void None %3
 %5 = OpLabel
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07035"));
}

TEST_F(ValidateBuiltIns, BadBuiltinCullPrimitiveEXTWithPerPrimitiveEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_CullPrimitiveEXT"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
 %ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07038"));
}

TEST_F(ValidateBuiltIns, BadBuiltinPrimitiveShadingRateWithPerPrimitiveEXT) {
  const std::string text = R"(
      OpCapability FragmentShadingRateKHR
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
      OpExtension "SPV_KHR_fragment_shading_rate"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_PrimitiveShadingRateKHR"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn PrimitiveShadingRateKHR
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-07059"));
}

TEST_F(ValidateBuiltIns, BadExecModelVulkanCullPrimitiveEXT) {
  const std::string text = R"(
         OpCapability MeshShadingNV
         OpCapability MeshShadingEXT
         OpExtension "SPV_NV_mesh_shader"
         OpExtension "SPV_EXT_mesh_shader"
    %1 = OpExtInstImport "GLSL.std.450"
         OpMemoryModel Logical GLSL450
         OpEntryPoint MeshNV %main "main" %gl_MeshPrimitivesEXT
         OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
         OpExecutionMode %main OutputVertices 81
         OpExecutionMode %main OutputPrimitivesNV 32
         OpExecutionMode %main OutputTrianglesNV
         OpSource GLSL 450
         OpSourceExtension "GL_EXT_mesh_shader"
         OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
         OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn CullPrimitiveEXT
         OpDecorate %gl_MeshPerPrimitiveEXT Block
 %void = OpTypeVoid
    %3 = OpTypeFunction %void
 %uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%v3uint = OpTypeVector %uint 3
 %bool = OpTypeBool
  %int = OpTypeInt 32 1
%int_0 = OpConstant %int 0
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_ptr_Output_bool = OpTypePointer Output %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
 %main = OpFunction %void None %3
    %5 = OpLabel
   %18 = OpAccessChain %_ptr_Output_bool %gl_MeshPrimitivesEXT %int_0 %int_0
         OpReturn
         OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-CullPrimitiveEXT-CullPrimitiveEXT-07034"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinLayerInBlockMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_Layer"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn Layer
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinLayerAsArrayOfIntMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_Layer
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpDecorate %gl_Layer BuiltIn Layer
      OpDecorate %gl_Layer PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_Layer_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_Layer_uint_32 = OpTypePointer Output %_arr_gl_Layer_uint_32
%gl_Layer = OpVariable %_ptr_Output__arr_gl_Layer_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinLayerArrayTypeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_Layer
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpDecorate %gl_Layer BuiltIn Layer
      OpDecorate %gl_Layer PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_Layer_uint_32 = OpTypeArray %bool %uint_32
%_ptr_Output__arr_gl_Layer_uint_32 = OpTypePointer Output %_arr_gl_Layer_uint_32
%gl_Layer = OpVariable %_ptr_Output__arr_gl_Layer_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_Layer_uint_32 %gl_Layer
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(), AnyVUID("VUID-Layer-Layer-10592"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinLayerInBlockMeshEXTType) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_Layer"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn Layer
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
 %ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(), AnyVUID("VUID-Layer-Layer-10592"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinLayerArrayOfIntSizeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_Layer
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 16
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpDecorate %gl_Layer BuiltIn Layer
      OpDecorate %gl_Layer PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_Layer_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_Layer_uint_32 = OpTypePointer Output %_arr_gl_Layer_uint_32
%gl_Layer = OpVariable %_ptr_Output__arr_gl_Layer_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_Layer_uint_32 %gl_Layer
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(), AnyVUID("VUID-Layer-Layer-10593"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinLayerInBlockArraySizeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 16
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
      OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_Layer"
      OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
      OpDecorate %gl_MeshPerPrimitiveEXT Block
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn Layer
      OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(), AnyVUID("VUID-Layer-Layer-10594"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinLayerWithPerPrimitiveEXT) {
  const std::string text = R"(
          OpCapability MeshShadingEXT
          OpCapability Shader
          OpExtension "SPV_EXT_mesh_shader"
          OpMemoryModel Logical GLSL450
          OpEntryPoint MeshEXT %MainMesh "MainMesh" %gl_Layer
          OpExecutionMode %MainMesh OutputPrimitivesNV 1
          OpExecutionMode %MainMesh OutputVertices 3
          OpExecutionMode %MainMesh OutputTrianglesNV
          OpExecutionMode %MainMesh LocalSize 1 1 1
          OpSource Slang 1
          OpName %MainMesh "MainMesh"
          OpDecorate %gl_Layer BuiltIn Layer
  %void = OpTypeVoid
     %9 = OpTypeFunction %void
  %uint = OpTypeInt 32 0
%uint_3 = OpConstant %uint 3
%uint_1 = OpConstant %uint 1
 %float = OpTypeFloat 32
   %int = OpTypeInt 32 1
 %int_1 = OpConstant %int 1
 %int_3 = OpConstant %int 3
%uint_0 = OpConstant %uint 0
%v3float = OpTypeVector %float 3
%_ptr_Output_v3float = OpTypePointer Output %v3float
%v3uint = OpTypeVector %uint 3
%_ptr_Output_v3uint = OpTypePointer Output %v3uint
%_ptr_Output_int = OpTypePointer Output %int
%_arr_int_int_1 = OpTypeArray %int %int_1
%_ptr_Output__arr_int_int_1 = OpTypePointer Output %_arr_int_int_1
%gl_Layer = OpVariable %_ptr_Output__arr_int_int_1 Output
%MainMesh = OpFunction %void None %9
    %25 = OpLabel
%ref_load = OpLoad %_arr_int_int_1 %gl_Layer
          OpSetMeshOutputsEXT %uint_3 %uint_1
          OpReturn
          OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(), AnyVUID("VUID-Layer-Layer-07039"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinPrimitiveShadingRateKHRInBlockMeshEXT) {
  const std::string text = R"(
    OpCapability FragmentShadingRateKHR
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
    OpExtension "SPV_KHR_fragment_shading_rate"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_PrimitiveShadingRateEXT"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn PrimitiveShadingRateKHR
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns,
       VulkanBuiltinPrimitiveShadingRateKHRInArrayOfIntMeshEXT) {
  const std::string text = R"(
      OpCapability FragmentShadingRateKHR
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
      OpExtension "SPV_KHR_fragment_shading_rate"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_PrimitiveShadingRateEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_PrimitiveShadingRateEXT "gl_PrimitiveShadingRateEXT"
      OpDecorate %gl_PrimitiveShadingRateEXT BuiltIn PrimitiveShadingRateKHR
      OpDecorate %gl_PrimitiveShadingRateEXT PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypePointer Output %_arr_gl_PrimitiveShadingRateEXT_uint_32
%gl_PrimitiveShadingRateEXT = OpVariable %_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_PrimitiveShadingRateEXT_uint_32 %gl_PrimitiveShadingRateEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns,
       BadVulkanBuiltinPrimitiveShadingRateKHRInArrayTypeMeshEXT) {
  const std::string text = R"(
      OpCapability FragmentShadingRateKHR
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
      OpExtension "SPV_KHR_fragment_shading_rate"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_PrimitiveShadingRateEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_PrimitiveShadingRateEXT "gl_PrimitiveShadingRateEXT"
      OpDecorate %gl_PrimitiveShadingRateEXT BuiltIn PrimitiveShadingRateKHR
      OpDecorate %gl_PrimitiveShadingRateEXT PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypeArray %bool %uint_32
%_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypePointer Output %_arr_gl_PrimitiveShadingRateEXT_uint_32
%gl_PrimitiveShadingRateEXT = OpVariable %_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
 %ref_load = OpLoad %_arr_gl_PrimitiveShadingRateEXT_uint_32 %gl_PrimitiveShadingRateEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-10598"));
}

TEST_F(ValidateBuiltIns,
       BadVulkanBuiltinPrimitiveShadingRateKHRInBlockTypeMeshEXT) {
  const std::string text = R"(
    OpCapability FragmentShadingRateKHR
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
    OpExtension "SPV_KHR_fragment_shading_rate"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_PrimitiveShadingRateEXT"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn PrimitiveShadingRateKHR
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-10598"));
}

TEST_F(ValidateBuiltIns,
       BadVulkanBuiltinPrimitiveShadingRateKHRInBlockSizeMeshEXT) {
  const std::string text = R"(
    OpCapability FragmentShadingRateKHR
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
    OpExtension "SPV_KHR_fragment_shading_rate"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 16
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_PrimitiveShadingRateEXT"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn PrimitiveShadingRateKHR
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-10600"));
}

TEST_F(ValidateBuiltIns,
       BadVulkanBuiltinPrimitiveShadingRateKHRInArraySizeMeshEXT) {
  const std::string text = R"(
      OpCapability FragmentShadingRateKHR
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
      OpExtension "SPV_KHR_fragment_shading_rate"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_PrimitiveShadingRateEXT
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 16
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_PrimitiveShadingRateEXT "gl_PrimitiveShadingRateEXT"
      OpDecorate %gl_PrimitiveShadingRateEXT BuiltIn PrimitiveShadingRateKHR
      OpDecorate %gl_PrimitiveShadingRateEXT PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 = OpTypePointer Output %_arr_gl_PrimitiveShadingRateEXT_uint_32
%gl_PrimitiveShadingRateEXT = OpVariable %_ptr_Output__arr_gl_PrimitiveShadingRateEXT_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_PrimitiveShadingRateEXT_uint_32 %gl_PrimitiveShadingRateEXT
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      AnyVUID("VUID-PrimitiveShadingRateKHR-PrimitiveShadingRateKHR-10599"));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinViewportIndexInBlockMeshEXT) {
  const std::string text = R"(
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_ViewportIndex"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn ViewportIndex
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, VulkanBuiltinViewportIndexAsArrayOfIntMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_ViewportIndex
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_ViewportIndex "gl_ViewportIndex"
      OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
      OpDecorate %gl_ViewportIndex PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_ViewportIndex_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_ViewportIndex_uint_32 = OpTypePointer Output %_arr_gl_ViewportIndex_uint_32
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_gl_ViewportIndex_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinViewportIndexInBlockTypeMeshEXT) {
  const std::string text = R"(
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 32
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_ViewportIndex"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn ViewportIndex
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %bool
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-ViewportIndex-ViewportIndex-10601"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinViewportIndexAsArrayTypeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_ViewportIndex
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 32
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_ViewportIndex "gl_ViewportIndex"
      OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
      OpDecorate %gl_ViewportIndex PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_ViewportIndex_uint_32 = OpTypeArray %bool %uint_32
%_ptr_Output__arr_gl_ViewportIndex_uint_32 = OpTypePointer Output %_arr_gl_ViewportIndex_uint_32
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_gl_ViewportIndex_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_ViewportIndex_uint_32 %gl_ViewportIndex
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-ViewportIndex-ViewportIndex-10601"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinViewportIndexInBlockArraySizeMeshEXT) {
  const std::string text = R"(
    OpCapability MeshShadingEXT
    OpExtension "SPV_EXT_mesh_shader"
%1 = OpExtInstImport "GLSL.std.450"
    OpMemoryModel Logical GLSL450
    OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
    OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
    OpExecutionMode %main OutputVertices 81
    OpExecutionMode %main OutputPrimitivesEXT 16
    OpExecutionMode %main OutputTrianglesEXT
    OpSource GLSL 450
    OpSourceExtension "GL_EXT_mesh_shader"
    OpName %main "main"
    OpName %gl_MeshPerPrimitiveEXT "gl_MeshPerPrimitiveEXT"
    OpMemberName %gl_MeshPerPrimitiveEXT 0 "gl_ViewportIndex"
    OpName %gl_MeshPrimitivesEXT "gl_MeshPrimitivesEXT"
    OpDecorate %gl_MeshPerPrimitiveEXT Block
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn ViewportIndex
    OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
%void = OpTypeVoid
%3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%gl_MeshPerPrimitiveEXT = OpTypeStruct %int
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%main = OpFunction %void None %3
%5 = OpLabel
%ref_load = OpLoad %_arr_gl_MeshPerPrimitiveEXT_uint_32 %gl_MeshPrimitivesEXT
    OpReturn
    OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-ViewportIndex-ViewportIndex-10603"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinViewportIndexAsArrayOfIntSizeMeshEXT) {
  const std::string text = R"(
      OpCapability MeshShadingEXT
      OpExtension "SPV_EXT_mesh_shader"
 %1 = OpExtInstImport "GLSL.std.450"
      OpMemoryModel Logical GLSL450
      OpEntryPoint MeshEXT %main "main" %gl_ViewportIndex
      OpExecutionModeId %main LocalSizeId %uint_32 %uint_1 %uint_1
      OpExecutionMode %main OutputVertices 81
      OpExecutionMode %main OutputPrimitivesEXT 16
      OpExecutionMode %main OutputTrianglesEXT
      OpSource GLSL 450
      OpSourceExtension "GL_EXT_mesh_shader"
      OpName %main "main"
      OpName %gl_ViewportIndex "gl_ViewportIndex"
      OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex
      OpDecorate %gl_ViewportIndex PerPrimitiveEXT
%void = OpTypeVoid
 %3 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%uint_1 = OpConstant %uint 1
%int = OpTypeInt 32 1
%bool = OpTypeBool
%_arr_gl_ViewportIndex_uint_32 = OpTypeArray %int %uint_32
%_ptr_Output__arr_gl_ViewportIndex_uint_32 = OpTypePointer Output %_arr_gl_ViewportIndex_uint_32
%gl_ViewportIndex = OpVariable %_ptr_Output__arr_gl_ViewportIndex_uint_32 Output
%main = OpFunction %void None %3
 %5 = OpLabel
%ref_load = OpLoad %_arr_gl_ViewportIndex_uint_32 %gl_ViewportIndex
      OpReturn
      OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-ViewportIndex-ViewportIndex-10602"));
}

TEST_F(ValidateBuiltIns, BadVulkanBuiltinPrimitiveIdFragmentWithRayTracing) {
  const std::string text = R"(
          OpCapability Shader
          OpCapability RayTracingKHR
          OpExtension "SPV_KHR_ray_tracing"
          OpMemoryModel Logical GLSL450
          OpEntryPoint Fragment %main "main" %outVar %gl_PrimitiveID
          OpExecutionMode %main OriginUpperLeft
          OpDecorate %outVar Location 0
          OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
          OpDecorate %gl_PrimitiveID Flat
  %void = OpTypeVoid
     %4 = OpTypeFunction %void
   %int = OpTypeInt 32 1
 %v4int = OpTypeVector %int 4
%ptrOut = OpTypePointer Output %v4int
%outVar = OpVariable %ptrOut Output
 %ptrIn = OpTypePointer Input %int
%gl_PrimitiveID = OpVariable %ptrIn Input
  %main = OpFunction %void None %4
     %6 = OpLabel
    %13 = OpLoad %int %gl_PrimitiveID
    %14 = OpCompositeConstruct %v4int %13 %13 %13 %13
          OpStore %outVar %14
          OpReturn
          OpFunctionEnd
)";

  CompileSuccessfully(text, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-PrimitiveId-PrimitiveId-04333"));
}

TEST_F(ValidateBuiltIns, TessellationMissingPatch) {
  const std::string spirv = R"(
               OpCapability Tessellation
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationControl %main "main" %gl_TessLevelInner %gl_TessLevelOuter
               OpExecutionMode %main OutputVertices 3
               OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
               OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
               OpDecorate %gl_TessLevelOuter Patch
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%_ptr_Output__arr_float_uint_2 = OpTypePointer Output %_arr_float_uint_2
%gl_TessLevelInner = OpVariable %_ptr_Output__arr_float_uint_2 Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %float_1 = OpConstant %float 1
%_ptr_Output_float = OpTypePointer Output %float
     %uint_4 = OpConstant %uint 4
%_arr_float_uint_4 = OpTypeArray %float %uint_4
%_ptr_Output__arr_float_uint_4 = OpTypePointer Output %_arr_float_uint_4
%gl_TessLevelOuter = OpVariable %_ptr_Output__arr_float_uint_4 Output
       %main = OpFunction %void None %4
          %6 = OpLabel
         %17 = OpAccessChain %_ptr_Output_float %gl_TessLevelInner %int_0
               OpStore %17 %float_1
         %22 = OpAccessChain %_ptr_Output_float %gl_TessLevelOuter %int_0
               OpStore %22 %float_1
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("BuiltIn TessLevelInner variable needs to also have a "
                        "Patch decoration"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-TessLevelInner-10880"));
}

// From dEQP-VK.mesh_shader.ext.builtin.primitive_id_spirv
TEST_F(ValidateBuiltIns, PrimitiveIdInFragmentWithMeshCapability) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability MeshShadingEXT
               OpExtension "SPV_EXT_mesh_shader"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main" %9 %gl_PrimitiveID
               OpExecutionMode %4 OriginUpperLeft
               OpDecorate %9 Location 0
               OpDecorate %gl_PrimitiveID Flat
               OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
          %9 = OpVariable %_ptr_Output_v4float Output
        %int = OpTypeInt 32 1
%_ptr_Input_int = OpTypePointer Input %int
%gl_PrimitiveID = OpVariable %_ptr_Input_int Input
%int_1629198956 = OpConstant %int 1629198956
       %bool = OpTypeBool
    %float_0 = OpConstant %float 0
    %float_1 = OpConstant %float 1
         %19 = OpConstantComposite %v4float %float_0 %float_0 %float_1 %float_1
         %20 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1
     %v4bool = OpTypeVector %bool 4
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpLoad %int %gl_PrimitiveID
         %16 = OpIEqual %bool %13 %int_1629198956
         %22 = OpCompositeConstruct %v4bool %16 %16 %16 %16
         %23 = OpSelect %v4float %22 %19 %20
               OpStore %9 %23
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

// https://github.com/KhronosGroup/SPIRV-Tools/issues/6237
TEST_F(ValidateBuiltIns, MeshBuiltinUnsignedInt) {
  const std::string spirv = R"(
               OpCapability FragmentShadingRateKHR
               OpCapability MeshShadingEXT
               OpExtension "SPV_EXT_mesh_shader"
               OpExtension "SPV_KHR_fragment_shading_rate"
               OpMemoryModel Logical GLSL450
               OpEntryPoint MeshEXT %main "main" %gl_MeshPrimitivesEXT
               OpExecutionModeId %main LocalSizeId %uint_1 %uint_1 %uint_1
               OpExecutionMode %main OutputVertices 81
               OpExecutionMode %main OutputPrimitivesEXT 32
               OpExecutionMode %main OutputTrianglesEXT
               OpDecorate %gl_MeshPerPrimitiveEXT Block
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 BuiltIn PrimitiveId
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 0 PerPrimitiveEXT
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 1 BuiltIn Layer
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 1 PerPrimitiveEXT
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 2 BuiltIn ViewportIndex
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 2 PerPrimitiveEXT
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 3 BuiltIn CullPrimitiveEXT
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 3 PerPrimitiveEXT
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 4 BuiltIn PrimitiveShadingRateKHR
               OpMemberDecorate %gl_MeshPerPrimitiveEXT 4 PerPrimitiveEXT
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
        %int = OpTypeInt 32 1
       %bool = OpTypeBool
      %int_0 = OpConstant %int 0
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
     %uint_4 = OpConstant %uint 4
    %uint_81 = OpConstant %uint 81
    %uint_32 = OpConstant %uint 32
%gl_MeshPerPrimitiveEXT = OpTypeStruct %uint %uint %uint %bool %uint
%_arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypeArray %gl_MeshPerPrimitiveEXT %uint_32
%_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 = OpTypePointer Output %_arr_gl_MeshPerPrimitiveEXT_uint_32
%gl_MeshPrimitivesEXT = OpVariable %_ptr_Output__arr_gl_MeshPerPrimitiveEXT_uint_32 Output
%_ptr_Output_uint = OpTypePointer Output %uint
       %main = OpFunction %void None %4
          %6 = OpLabel
               OpSetMeshOutputsEXT %uint_81 %uint_32
         %20 = OpAccessChain %_ptr_Output_uint %gl_MeshPrimitivesEXT %int_0 %uint_0
               OpStore %20 %uint_1
         %22 = OpAccessChain %_ptr_Output_uint %gl_MeshPrimitivesEXT %int_0 %uint_1
               OpStore %22 %uint_2
         %24 = OpAccessChain %_ptr_Output_uint %gl_MeshPrimitivesEXT %int_0 %uint_2
               OpStore %24 %uint_3
         %26 = OpAccessChain %_ptr_Output_uint %gl_MeshPrimitivesEXT %int_0 %uint_4
               OpStore %26 %uint_4
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

}  // namespace
}  // namespace val
}  // namespace spvtools
