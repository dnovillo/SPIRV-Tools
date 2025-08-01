// Copyright (c) 2017 Google Inc.
// Modifications Copyright (C) 2024 Advanced Micro Devices, Inc. All rights
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

// Validation tests for decorations

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "source/val/decoration.h"
#include "test/unit_spirv.h"
#include "test/val/val_code_generator.h"
#include "test/val/val_fixtures.h"

namespace spvtools {
namespace val {
namespace {

using ::testing::Combine;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Values;

struct TestResult {
  TestResult(spv_result_t in_validation_result = SPV_SUCCESS,
             const std::string& in_error_str = "")
      : validation_result(in_validation_result), error_str(in_error_str) {}
  spv_result_t validation_result;
  const std::string error_str;
};

using ValidateDecorations = spvtest::ValidateBase<bool>;
using ValidateDecorationString = spvtest::ValidateBase<std::string>;
using ValidateVulkanCombineDecorationResult =
    spvtest::ValidateBase<std::tuple<const char*, const char*, TestResult>>;

TEST_F(ValidateDecorations, ValidateOpDecorateRegistration) {
  std::string spirv = R"(
    OpCapability Shader
    OpCapability Linkage
    OpMemoryModel Logical GLSL450
    OpDecorate %1 Location 4
    OpDecorate %1 Centroid
    %2 = OpTypeFloat 32
    %3 = OpTypePointer Output %2
    %1 = OpVariable %3 Output
    ; Since %1 is used first in Decoration, it gets id 1.
)";
  const uint32_t id = 1;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  // Must have 2 decorations.
  EXPECT_THAT(
      vstate_->id_decorations(id),
      Eq(std::set<Decoration>{Decoration(spv::Decoration::Location, {4}),
                              Decoration(spv::Decoration::Centroid)}));
}

TEST_F(ValidateDecorations, ValidateOpMemberDecorateRegistration) {
  std::string spirv = R"(
    OpCapability Shader
    OpCapability Linkage
    OpMemoryModel Logical GLSL450
    OpDecorate %_arr_double_uint_6 ArrayStride 4
    OpMemberDecorate %_struct_115 2 NonReadable
    OpMemberDecorate %_struct_115 2 Offset 2
    OpDecorate %_struct_115 BufferBlock
    %float = OpTypeFloat 32
    %uint = OpTypeInt 32 0
    %uint_6 = OpConstant %uint 6
    %_arr_double_uint_6 = OpTypeArray %float %uint_6
    %_struct_115 = OpTypeStruct %float %float %_arr_double_uint_6
)";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());

  // The array must have 1 decoration.
  const uint32_t arr_id = 1;
  EXPECT_THAT(
      vstate_->id_decorations(arr_id),
      Eq(std::set<Decoration>{Decoration(spv::Decoration::ArrayStride, {4})}));

  // The struct must have 3 decorations.
  const uint32_t struct_id = 2;
  EXPECT_THAT(
      vstate_->id_decorations(struct_id),
      Eq(std::set<Decoration>{Decoration(spv::Decoration::NonReadable, {}, 2),
                              Decoration(spv::Decoration::Offset, {2}, 2),
                              Decoration(spv::Decoration::BufferBlock)}));
}

TEST_F(ValidateDecorations, ValidateOpMemberDecorateOutOfBound) {
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %1 "Main"
               OpExecutionMode %1 OriginUpperLeft
               OpMemberDecorate %_struct_2 1 RelaxedPrecision
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
      %float = OpTypeFloat 32
  %_struct_2 = OpTypeStruct %float
          %1 = OpFunction %void None %4
          %6 = OpLabel
               OpReturn
               OpFunctionEnd
)";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Index 1 provided in OpMemberDecorate for struct <id> "
                        "'2[%_struct_2]' is out of bounds. The structure has 1 "
                        "members. Largest valid index is 0."));
}

TEST_F(ValidateDecorations, ValidateGroupDecorateRegistration) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %1 DescriptorSet 0
               OpDecorate %1 RelaxedPrecision
               OpDecorate %1 Restrict
          %1 = OpDecorationGroup
               OpGroupDecorate %1 %2 %3
               OpGroupDecorate %1 %4
  %float = OpTypeFloat 32
%_runtimearr_float = OpTypeRuntimeArray %float
  %_struct_9 = OpTypeStruct %_runtimearr_float
%_ptr_Uniform__struct_9 = OpTypePointer Uniform %_struct_9
         %2 = OpVariable %_ptr_Uniform__struct_9 Uniform
 %_struct_10 = OpTypeStruct %_runtimearr_float
%_ptr_Uniform__struct_10 = OpTypePointer Uniform %_struct_10
         %3 = OpVariable %_ptr_Uniform__struct_10 Uniform
 %_struct_11 = OpTypeStruct %_runtimearr_float
%_ptr_Uniform__struct_11 = OpTypePointer Uniform %_struct_11
         %4 = OpVariable %_ptr_Uniform__struct_11 Uniform
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());

  // Decoration group has 3 decorations.
  auto expected_decorations =
      std::set<Decoration>{Decoration(spv::Decoration::DescriptorSet, {0}),
                           Decoration(spv::Decoration::RelaxedPrecision),
                           Decoration(spv::Decoration::Restrict)};

  // Decoration group is applied to id 1, 2, 3, and 4. Note that id 1 (which is
  // the decoration group id) also has all the decorations.
  EXPECT_THAT(vstate_->id_decorations(1), Eq(expected_decorations));
  EXPECT_THAT(vstate_->id_decorations(2), Eq(expected_decorations));
  EXPECT_THAT(vstate_->id_decorations(3), Eq(expected_decorations));
  EXPECT_THAT(vstate_->id_decorations(4), Eq(expected_decorations));
}

TEST_F(ValidateDecorations, ValidateGroupMemberDecorateRegistration) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %1 Offset 3
          %1 = OpDecorationGroup
               OpGroupMemberDecorate %1 %_struct_1 3 %_struct_2 3 %_struct_3 3
      %float = OpTypeFloat 32
%_runtimearr = OpTypeRuntimeArray %float
  %_struct_1 = OpTypeStruct %float %float %float %_runtimearr
  %_struct_2 = OpTypeStruct %float %float %float %_runtimearr
  %_struct_3 = OpTypeStruct %float %float %float %_runtimearr
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  // Decoration group has 1 decoration.
  auto expected_decorations =
      std::set<Decoration>{Decoration(spv::Decoration::Offset, {3}, 3)};

  // Decoration group is applied to id 2, 3, and 4.
  EXPECT_THAT(vstate_->id_decorations(2), Eq(expected_decorations));
  EXPECT_THAT(vstate_->id_decorations(3), Eq(expected_decorations));
  EXPECT_THAT(vstate_->id_decorations(4), Eq(expected_decorations));
}

TEST_F(ValidateDecorations, LinkageImportUsedForInitializedVariableBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %target LinkageAttributes "link_ptr" Import
      %float = OpTypeFloat 32
 %_ptr_float = OpTypePointer Uniform %float
       %zero = OpConstantNull %float
     %target = OpVariable %_ptr_float Uniform %zero
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("A module-scope OpVariable with initialization value "
                        "cannot be marked with the Import Linkage Type."));
}
TEST_F(ValidateDecorations, LinkageExportUsedForInitializedVariableGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %target LinkageAttributes "link_ptr" Export
      %float = OpTypeFloat 32
 %_ptr_float = OpTypePointer Uniform %float
       %zero = OpConstantNull %float
     %target = OpVariable %_ptr_float Uniform %zero
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, StructAllMembersHaveBuiltInDecorationsGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %_struct_1 Block
               OpMemberDecorate %_struct_1 0 BuiltIn Position
               OpMemberDecorate %_struct_1 1 BuiltIn Position
               OpMemberDecorate %_struct_1 2 BuiltIn Position
               OpMemberDecorate %_struct_1 3 BuiltIn Position
      %float = OpTypeFloat 32
%_runtimearr = OpTypeRuntimeArray %float
  %_struct_1 = OpTypeStruct %float %float %float %_runtimearr
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, MixedBuiltInDecorationsBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %_struct_1 Block
               OpMemberDecorate %_struct_1 0 BuiltIn Position
               OpMemberDecorate %_struct_1 1 BuiltIn Position
      %float = OpTypeFloat 32
%_runtimearr = OpTypeRuntimeArray %float
  %_struct_1 = OpTypeStruct %float %float %float %_runtimearr
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("When BuiltIn decoration is applied to a structure-type "
                "member, all members of that structure type must also be "
                "decorated with BuiltIn (No allowed mixing of built-in "
                "variables and non-built-in variables within a single "
                "structure). Structure id 1 does not meet this requirement."));
}

TEST_F(ValidateDecorations, StructContainsBuiltInStructBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %_struct_1 Block
               OpMemberDecorate %_struct_1 0 BuiltIn Position
               OpMemberDecorate %_struct_1 1 BuiltIn Position
               OpMemberDecorate %_struct_1 2 BuiltIn Position
               OpMemberDecorate %_struct_1 3 BuiltIn Position
      %float = OpTypeFloat 32
%_runtimearr = OpTypeRuntimeArray %float
  %_struct_1 = OpTypeStruct %float %float %float %_runtimearr
  %_struct_2 = OpTypeStruct %_struct_1
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Structure <id> '1[%_struct_1]' contains members with "
                "BuiltIn decoration. Therefore this structure may not "
                "be contained as a member of another structure type. "
                "Structure <id> '4[%_struct_4]' contains structure <id> "
                "'1[%_struct_1]'."));
}

TEST_F(ValidateDecorations, StructContainsNonBuiltInStructGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
      %float = OpTypeFloat 32
  %_struct_1 = OpTypeStruct %float
  %_struct_2 = OpTypeStruct %_struct_1
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, MultipleBuiltInObjectsConsumedByOpEntryPointBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Geometry
               OpMemoryModel Logical GLSL450
               OpEntryPoint Geometry %main "main" %in_1 %in_2
               OpExecutionMode %main InputPoints
               OpExecutionMode %main OutputPoints
               OpDecorate %struct_1 Block
               OpDecorate %struct_2 Block
               OpMemberDecorate %struct_1 0 BuiltIn InvocationId
               OpMemberDecorate %struct_2 0 BuiltIn Position
      %int = OpTypeInt 32 1
     %void = OpTypeVoid
     %func = OpTypeFunction %void
    %float = OpTypeFloat 32
 %struct_1 = OpTypeStruct %int
 %struct_2 = OpTypeStruct %float
%ptr_builtin_1 = OpTypePointer Input %struct_1
%ptr_builtin_2 = OpTypePointer Input %struct_2
%in_1 = OpVariable %ptr_builtin_1 Input
%in_2 = OpVariable %ptr_builtin_2 Input
       %main = OpFunction %void None %func
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("There must be at most one object per Storage Class "
                        "that can contain a structure type containing members "
                        "decorated with BuiltIn, consumed per entry-point."));
}

TEST_F(ValidateDecorations,
       OneBuiltInObjectPerStorageClassConsumedByOpEntryPointGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Geometry
               OpMemoryModel Logical GLSL450
               OpEntryPoint Geometry %main "main" %in_1 %out_1
               OpExecutionMode %main InputPoints
               OpExecutionMode %main OutputPoints
               OpDecorate %struct_1 Block
               OpDecorate %struct_2 Block
               OpMemberDecorate %struct_1 0 BuiltIn InvocationId
               OpMemberDecorate %struct_2 0 BuiltIn Position
      %int = OpTypeInt 32 1
     %void = OpTypeVoid
     %func = OpTypeFunction %void
    %float = OpTypeFloat 32
 %struct_1 = OpTypeStruct %int
 %struct_2 = OpTypeStruct %float
%ptr_builtin_1 = OpTypePointer Input %struct_1
%ptr_builtin_2 = OpTypePointer Output %struct_2
%in_1 = OpVariable %ptr_builtin_1 Input
%out_1 = OpVariable %ptr_builtin_2 Output
       %main = OpFunction %void None %func
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, NoBuiltInObjectsConsumedByOpEntryPointGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Geometry
               OpMemoryModel Logical GLSL450
               OpEntryPoint Geometry %main "main" %in_1 %out_1
               OpExecutionMode %main InputPoints
               OpExecutionMode %main OutputPoints
      %int = OpTypeInt 32 1
     %void = OpTypeVoid
     %func = OpTypeFunction %void
    %float = OpTypeFloat 32
 %struct_1 = OpTypeStruct %int
 %struct_2 = OpTypeStruct %float
%ptr_builtin_1 = OpTypePointer Input %struct_1
%ptr_builtin_2 = OpTypePointer Output %struct_2
%in_1 = OpVariable %ptr_builtin_1 Input
%out_1 = OpVariable %ptr_builtin_2 Output
       %main = OpFunction %void None %func
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, EntryPointFunctionHasLinkageAttributeBad) {
  std::string spirv = R"(
      OpCapability Shader
      OpCapability Linkage
      OpMemoryModel Logical GLSL450
      OpEntryPoint GLCompute %main "main"
      OpDecorate %main LinkageAttributes "import_main" Import
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%main = OpFunction %1 None %2
%4 = OpLabel
     OpReturn
     OpFunctionEnd
)";
  CompileSuccessfully(spirv.c_str());
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("The LinkageAttributes Decoration (Linkage name: import_main) "
                "cannot be applied to function id 1 because it is targeted by "
                "an OpEntryPoint instruction."));
}

TEST_F(ValidateDecorations, FunctionDeclarationWithoutImportLinkageBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
     %void = OpTypeVoid
     %func = OpTypeFunction %void
       %main = OpFunction %void None %func
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Function declaration (id 3) must have a LinkageAttributes "
                "decoration with the Import Linkage type."));
}

TEST_F(ValidateDecorations, FunctionDeclarationWithImportLinkageGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %main LinkageAttributes "link_fn" Import
     %void = OpTypeVoid
     %func = OpTypeFunction %void
       %main = OpFunction %void None %func
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FunctionDeclarationWithExportLinkageBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %main LinkageAttributes "link_fn" Export
     %void = OpTypeVoid
     %func = OpTypeFunction %void
       %main = OpFunction %void None %func
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Function declaration (id 1) must have a LinkageAttributes "
                "decoration with the Import Linkage type."));
}

TEST_F(ValidateDecorations, FunctionDefinitionWithImportLinkageBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
               OpDecorate %main LinkageAttributes "link_fn" Import
     %void = OpTypeVoid
     %func = OpTypeFunction %void
       %main = OpFunction %void None %func
      %label = OpLabel
               OpReturn
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Function definition (id 1) may not be decorated with "
                        "Import Linkage type."));
}

TEST_F(ValidateDecorations, FunctionDefinitionWithoutImportLinkageGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Linkage
               OpMemoryModel Logical GLSL450
     %void = OpTypeVoid
     %func = OpTypeFunction %void
       %main = OpFunction %void None %func
      %label = OpLabel
               OpReturn
               OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, BuiltinVariablesGoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragCoord %_entryPointOutput
OpExecutionMode %main OriginUpperLeft
OpSource HLSL 500
OpDecorate %gl_FragCoord BuiltIn FragCoord
OpDecorate %_entryPointOutput Location 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%float_0 = OpConstant %float 0
%14 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_entryPointOutput = OpVariable %_ptr_Output_v4float Output
%main = OpFunction %void None %3
%5 = OpLabel
OpStore %_entryPointOutput %14
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, BuiltinVariablesWithLocationDecorationVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragCoord %_entryPointOutput
OpExecutionMode %main OriginUpperLeft
OpSource HLSL 500
OpDecorate %gl_FragCoord BuiltIn FragCoord
OpDecorate %gl_FragCoord Location 0
OpDecorate %_entryPointOutput Location 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%float_0 = OpConstant %float 0
%14 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_entryPointOutput = OpVariable %_ptr_Output_v4float Output
%main = OpFunction %void None %3
%5 = OpLabel
OpStore %_entryPointOutput %14
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04915"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("A BuiltIn variable (id 2) cannot have any Location or "
                        "Component decorations"));
}
TEST_F(ValidateDecorations, BuiltinVariablesWithComponentDecorationVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragCoord %_entryPointOutput
OpExecutionMode %main OriginUpperLeft
OpSource HLSL 500
OpDecorate %gl_FragCoord BuiltIn FragCoord
OpDecorate %gl_FragCoord Component 0
OpDecorate %_entryPointOutput Location 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%float_0 = OpConstant %float 0
%14 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_entryPointOutput = OpVariable %_ptr_Output_v4float Output
%main = OpFunction %void None %3
%5 = OpLabel
OpStore %_entryPointOutput %14
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04915"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("A BuiltIn variable (id 2) cannot have any Location or "
                        "Component decorations"));
}

TEST_F(ValidateDecorations, LocationDecorationOnNumericTypeBad) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %fragCoord Location 0
               OpDecorate %v4float Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
%non_interface = OpVariable %ptr_v4float Output
       %main = OpFunction %void None %voidfn
      %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Location decoration on target <id> '3[%v4float]' must "
                        "be a variable"));
}

TEST_F(ValidateDecorations, LocationDecorationOnStructBad) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %fragCoord Location 0
               OpDecorate %struct Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
     %struct = OpTypeStruct %float
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
%non_interface = OpVariable %ptr_v4float Output
       %main = OpFunction %void None %voidfn
      %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Location decoration on target <id> '3[%_struct_3]' "
                        "must be a variable"));
}

TEST_F(ValidateDecorations,
       LocationDecorationUnusedNonInterfaceVariableVulkan_Ignored) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
               OpDecorate %non_interface Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
%non_interface = OpVariable %ptr_v4float Output
       %main = OpFunction %void None %voidfn
      %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_EQ(getDiagnosticString(), "");
}

TEST_F(ValidateDecorations,
       LocationDecorationNonInterfaceStructVulkan_Ignored) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
              OpCapability Shader
              OpMemoryModel Logical GLSL450
              OpEntryPoint Fragment %main "main" %fragCoord
              OpExecutionMode %main OriginUpperLeft
              OpDecorate %fragCoord Location 0
              OpMemberDecorate %block 0 Location 2
              OpMemberDecorate %block 0 Component 1
              OpDecorate %block Block
      %void = OpTypeVoid
    %voidfn = OpTypeFunction %void
     %float = OpTypeFloat 32
      %vec3 = OpTypeVector %float 3
%outvar_ptr = OpTypePointer Output %vec3
 %fragCoord = OpVariable %outvar_ptr Output
     %block = OpTypeStruct %vec3
 %invar_ptr = OpTypePointer Input %block
%non_interface = OpVariable %invar_ptr Input
      %main = OpFunction %void None %voidfn
     %label = OpLabel
              OpReturn
              OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_EQ(getDiagnosticString(), "");
}

TEST_F(ValidateDecorations, LocationDecorationNonInterfaceStructVulkanGood) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
              OpCapability Shader
              OpMemoryModel Logical GLSL450
              OpEntryPoint Fragment %main "main" %fragCoord %interface
              OpExecutionMode %main OriginUpperLeft
              OpDecorate %fragCoord Location 0
              OpMemberDecorate %block 0 Location 2
              OpMemberDecorate %block 0 Component 1
              OpDecorate %block Block
      %void = OpTypeVoid
    %voidfn = OpTypeFunction %void
     %float = OpTypeFloat 32
      %vec3 = OpTypeVector %float 3
%outvar_ptr = OpTypePointer Output %vec3
 %fragCoord = OpVariable %outvar_ptr Output
     %block = OpTypeStruct %vec3
 %invar_ptr = OpTypePointer Input %block
 %interface = OpVariable %invar_ptr Input ;; this variable is unused. Ignore it
      %main = OpFunction %void None %voidfn
     %label = OpLabel
              OpReturn
              OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, LocationDecorationVariableNonStructVulkanBad) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %nonblock_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
%nonblock_var = OpVariable %ptr_v4float Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04916"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Variable must be decorated with a location"));
}

TEST_F(ValidateDecorations, LocationDecorationVariableStructNoBlockVulkanBad) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %block_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
      %block = OpTypeStruct %v4float
  %block_ptr = OpTypePointer Output %block
  %block_var = OpVariable %block_ptr Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04917"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Variable must be decorated with a location"));
}

TEST_F(ValidateDecorations, LocationDecorationVariableNoBlockVulkanGood) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %block_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
               OpDecorate %block_var Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
      %block = OpTypeStruct %v4float
  %block_ptr = OpTypePointer Output %block
  %block_var = OpVariable %block_ptr Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, LocationDecorationVariableExtraMemeberVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %block_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
               OpDecorate %block Block
               OpDecorate %block_var Location 1
               OpMemberDecorate %block 0 Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
      %block = OpTypeStruct %v4float
  %block_ptr = OpTypePointer Output %block
  %block_var = OpVariable %block_ptr Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd

)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04918"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Members cannot be assigned a location"));
}

TEST_F(ValidateDecorations, LocationDecorationVariableMissingMemeberVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %block_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
               OpDecorate %block Block
               OpMemberDecorate %block 0 Location 1
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
      %block = OpTypeStruct %v4float %v4float
  %block_ptr = OpTypePointer Output %block
  %block_var = OpVariable %block_ptr Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Location-04919"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Member index 1 is missing a location assignment"));
}

TEST_F(ValidateDecorations, LocationDecorationVariableOnlyMemeberVulkanGood) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragCoord %block_var
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %fragCoord Location 0
               OpDecorate %block Block
               OpMemberDecorate %block 0 Location 1
               OpMemberDecorate %block 1 Location 4
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_v4float = OpTypePointer Output %v4float
  %fragCoord = OpVariable %ptr_v4float Output
      %block = OpTypeStruct %v4float %v4float
  %block_ptr = OpTypePointer Output %block
  %block_var = OpVariable %block_ptr Output
       %main = OpFunction %void None %voidfn
       %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

// #version 440
// #extension GL_EXT_nonuniform_qualifier : enable
// layout(binding = 1) uniform sampler2D s2d[];
// layout(location = 0) in nonuniformEXT int i;
// void main()
// {
//     vec4 v = texture(s2d[i], vec2(0.3));
// }
TEST_F(ValidateDecorations, RuntimeArrayOfDescriptorSetsIsAllowed) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpCapability ShaderNonUniformEXT
               OpCapability RuntimeDescriptorArrayEXT
               OpCapability SampledImageArrayNonUniformIndexingEXT
               OpExtension "SPV_EXT_descriptor_indexing"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %i
               OpSource GLSL 440
               OpSourceExtension "GL_EXT_nonuniform_qualifier"
               OpName %main "main"
               OpName %v "v"
               OpName %s2d "s2d"
               OpName %i "i"
               OpDecorate %s2d DescriptorSet 0
               OpDecorate %s2d Binding 1
               OpDecorate %i Location 0
               OpDecorate %i NonUniformEXT
               OpDecorate %18 NonUniformEXT
               OpDecorate %21 NonUniformEXT
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
         %10 = OpTypeImage %float 2D 0 0 0 1 Unknown
         %11 = OpTypeSampledImage %10
%_runtimearr_11 = OpTypeRuntimeArray %11
%_ptr_Uniform__runtimearr_11 = OpTypePointer Uniform %_runtimearr_11
        %s2d = OpVariable %_ptr_Uniform__runtimearr_11 Uniform
        %int = OpTypeInt 32 1
%_ptr_Input_int = OpTypePointer Input %int
          %i = OpVariable %_ptr_Input_int Input
%_ptr_Uniform_11 = OpTypePointer Uniform %11
    %v2float = OpTypeVector %float 2
%float_0_300000012 = OpConstant %float 0.300000012
         %24 = OpConstantComposite %v2float %float_0_300000012 %float_0_300000012
    %float_0 = OpConstant %float 0
       %main = OpFunction %void None %3
          %5 = OpLabel
          %v = OpVariable %_ptr_Function_v4float Function
         %18 = OpLoad %int %i
         %20 = OpAccessChain %_ptr_Uniform_11 %s2d %18
         %21 = OpLoad %11 %20
         %26 = OpImageSampleExplicitLod %v4float %21 %24 Lod %float_0
               OpStore %v %26
               OpReturn
               OpFunctionEnd
)";
  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, BlockDecoratingArrayBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
     %Output = OpTypeArray %float %int_3
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a structure type"));
}

TEST_F(ValidateDecorations, BlockDecoratingIntBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
     %Output = OpTypeInt 32 1
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a structure type"));
}

TEST_F(ValidateDecorations, BlockMissingOffsetBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be explicitly laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, BufferBlockMissingOffsetBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be explicitly laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, BlockNestedStructMissingOffsetBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %v3float %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be explicitly laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, BufferBlockNestedStructMissingOffsetBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %v3float %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be explicitly laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, BlockGLSLSharedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
               OpDecorate %Output GLSLShared
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLShared' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BufferBlockGLSLSharedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
               OpDecorate %Output GLSLShared
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLShared' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BlockNestedStructGLSLSharedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %S GLSLShared
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLShared' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BufferBlockNestedStructGLSLSharedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %S GLSLShared
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLShared' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BlockGLSLPackedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
               OpDecorate %Output GLSLPacked
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLPacked' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BufferBlockGLSLPackedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
               OpDecorate %Output GLSLPacked
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLPacked' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BlockNestedStructGLSLPackedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %S GLSLPacked
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLPacked' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BufferBlockNestedStructGLSLPackedBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %S GLSLPacked
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
          %S = OpTypeStruct %int
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "'GLSLPacked' is not valid for the Vulkan execution environment"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[VUID-StandaloneSpirv-GLSLShared-04669]"));
}

TEST_F(ValidateDecorations, BlockMissingArrayStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %float %int_3
     %Output = OpTypeStruct %array
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with ArrayStride decorations"));
}

TEST_F(ValidateDecorations, BufferBlockMissingArrayStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %float %int_3
     %Output = OpTypeStruct %array
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with ArrayStride decorations"));
}

TEST_F(ValidateDecorations, BlockNestedStructMissingArrayStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %float %int_3
          %S = OpTypeStruct %array
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with ArrayStride decorations"));
}

TEST_F(ValidateDecorations, BufferBlockNestedStructMissingArrayStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %float %int_3
          %S = OpTypeStruct %array
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with ArrayStride decorations"));
}

TEST_F(ValidateDecorations, BlockMissingMatrixStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
     %matrix = OpTypeMatrix %v3float 4
     %Output = OpTypeStruct %matrix
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BufferBlockMissingMatrixStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
     %matrix = OpTypeMatrix %v3float 4
     %Output = OpTypeStruct %matrix
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BlockMissingMatrixStrideArrayBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output Block
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
     %matrix = OpTypeMatrix %v3float 4
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %matrix %int_3
     %Output = OpTypeStruct %matrix
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BufferBlockMissingMatrixStrideArrayBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %Output BufferBlock
               OpMemberDecorate %Output 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
     %matrix = OpTypeMatrix %v3float 4
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %array = OpTypeArray %matrix %int_3
     %Output = OpTypeStruct %matrix
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BlockNestedStructMissingMatrixStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
    %v4float = OpTypeVector %float 4
     %matrix = OpTypeMatrix %v3float 4
          %S = OpTypeStruct %matrix
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BufferBlockNestedStructMissingMatrixStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 16
               OpMemberDecorate %Output 2 Offset 32
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
    %v4float = OpTypeVector %float 4
     %matrix = OpTypeMatrix %v3float 4
          %S = OpTypeStruct %matrix
     %Output = OpTypeStruct %float %v4float %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, BlockStandardUniformBufferLayout) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %F 0 Offset 0
               OpMemberDecorate %F 1 Offset 8
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %_arr_mat3v3float_uint_2 ArrayStride 48
               OpMemberDecorate %O 0 Offset 0
               OpMemberDecorate %O 1 Offset 16
               OpMemberDecorate %O 2 Offset 32
               OpMemberDecorate %O 3 Offset 64
               OpMemberDecorate %O 4 ColMajor
               OpMemberDecorate %O 4 Offset 80
               OpMemberDecorate %O 4 MatrixStride 16
               OpDecorate %_arr_O_uint_2 ArrayStride 176
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpMemberDecorate %Output 2 Offset 16
               OpMemberDecorate %Output 3 Offset 32
               OpMemberDecorate %Output 4 Offset 48
               OpMemberDecorate %Output 5 Offset 64
               OpMemberDecorate %Output 6 ColMajor
               OpMemberDecorate %Output 6 Offset 96
               OpMemberDecorate %Output 6 MatrixStride 16
               OpMemberDecorate %Output 7 Offset 128
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
          %F = OpTypeStruct %int %v2uint
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%mat2v3float = OpTypeMatrix %v3float 2
     %v3uint = OpTypeVector %uint 3
%mat3v3float = OpTypeMatrix %v3float 3
%_arr_mat3v3float_uint_2 = OpTypeArray %mat3v3float %uint_2
          %O = OpTypeStruct %v3uint %v2float %_arr_float_uint_2 %v2float %_arr_mat3v3float_uint_2
%_arr_O_uint_2 = OpTypeArray %O %uint_2
     %Output = OpTypeStruct %float %v2float %v3float %F %float %_arr_float_uint_2 %mat2v3float %_arr_O_uint_2
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, BlockLayoutPermitsTightVec3ScalarPackingGood) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 12
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %v3float %float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, BlockCantAppearWithinABlockBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1587
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 16
               OpMemberDecorate %S2 0 Offset 0
               OpMemberDecorate %S2 1 Offset 12
               OpDecorate %S Block
               OpDecorate %S2 Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
         %S2 = OpTypeStruct %float %float
          %S = OpTypeStruct %float %S2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BufferblockCantAppearWithinABufferblockBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1587
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 16
              OpMemberDecorate %S2 0 Offset 0
               OpMemberDecorate %S2 1 Offset 16
               OpMemberDecorate %S3 0 Offset 0
               OpMemberDecorate %S3 1 Offset 12
               OpDecorate %S BufferBlock
               OpDecorate %S3 BufferBlock
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
         %S3 = OpTypeStruct %float %float
         %S2 = OpTypeStruct %float %S3
          %S = OpTypeStruct %float %S2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BufferblockCantAppearWithinABlockBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1587
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 16
              OpMemberDecorate %S2 0 Offset 0
               OpMemberDecorate %S2 1 Offset 16
               OpMemberDecorate %S3 0 Offset 0
               OpMemberDecorate %S3 1 Offset 12
               OpDecorate %S Block
               OpDecorate %S3 BufferBlock
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
         %S3 = OpTypeStruct %float %float
         %S2 = OpTypeStruct %float %S3
          %S = OpTypeStruct %float %S2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BlockCantAppearWithinABufferblockBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1587
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 16
              OpMemberDecorate %S2 0 Offset 0
               OpMemberDecorate %S2 1 Offset 16
              OpMemberDecorate %S3 0 Offset 0
               OpMemberDecorate %S3 1 Offset 16
               OpMemberDecorate %S4 0 Offset 0
               OpMemberDecorate %S4 1 Offset 12
               OpDecorate %S BufferBlock
               OpDecorate %S4 Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
         %S4 = OpTypeStruct %float %float
         %S3 = OpTypeStruct %float %S4
         %S2 = OpTypeStruct %float %S3
          %S = OpTypeStruct %float %S2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BlockCannotAppearWithinBlockArray) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpMemberDecorate %outer 0 Offset 0
OpMemberDecorate %outer 1 Offset 4
OpMemberDecorate %outer 2 Offset 20
OpDecorate %outer Block
OpMemberDecorate %inner 0 Offset 0
OpDecorate %inner Block
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%inner = OpTypeStruct %int
%array = OpTypeArray %inner %int_4
%outer = OpTypeStruct %int %array %int
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BlockCannotAppearWithinBlockMultiArray) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpMemberDecorate %outer 0 Offset 0
OpMemberDecorate %outer 1 Offset 4
OpDecorate %outer Block
OpMemberDecorate %inner 0 Offset 0
OpDecorate %inner Block
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%inner = OpTypeStruct %int
%array1 = OpTypeArray %inner %int_4
%array2 = OpTypeArray %array1 %int_4
%outer = OpTypeStruct %int %array2
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("rules: A Block or BufferBlock cannot be nested within "
                        "another Block or BufferBlock."));
}

TEST_F(ValidateDecorations, BlockLayoutForbidsTightScalarVec3PackingBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Structure id 2 decorated as Block for variable in Uniform "
                "storage class must follow standard uniform buffer layout "
                "rules: member 1 at offset 4 is not aligned to 16"));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsTightScalarVec3PackingWithRelaxedLayoutGood) {
  // Same as previous test, but with explicit option to relax block layout.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetRelaxBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsTightScalarVec3PackingBadOffsetWithRelaxedLayoutBad) {
  // Same as previous test, but with the vector not aligned to its scalar
  // element. Use offset 5 instead of a multiple of 4.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 5
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetRelaxBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in Uniform storage "
          "class must follow relaxed uniform buffer layout rules: member 1 at "
          "offset 5 is not aligned to scalar element size 4"));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsTightScalarVec3PackingWithVulkan1_1Good) {
  // Same as previous test, but with Vulkan 1.1.  Vulkan 1.1 included
  // VK_KHR_relaxed_block_layout in core.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsTightScalarVec3PackingWithScalarLayoutGood) {
  // Same as previous test, but with scalar block layout.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsScalarAlignedArrayWithScalarLayoutGood) {
  // The array at offset 4 is ok with scalar block layout.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
               OpDecorate %arr_float ArrayStride 4
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
      %float = OpTypeFloat 32
  %arr_float = OpTypeArray %float %uint_3
          %S = OpTypeStruct %float %arr_float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsScalarAlignedArrayOfVec3WithScalarLayoutGood) {
  // The array at offset 4 is ok with scalar block layout, even though
  // its elements are vec3.
  // This is the same as the previous case, but the array elements are vec3
  // instead of float.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
               OpDecorate %arr_vec3 ArrayStride 12
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
      %float = OpTypeFloat 32
       %vec3 = OpTypeVector %float 3
   %arr_vec3 = OpTypeArray %vec3 %uint_3
          %S = OpTypeStruct %float %arr_vec3
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations,
       BlockLayoutPermitsScalarAlignedStructWithScalarLayoutGood) {
  // Scalar block layout permits the struct at offset 4, even though
  // it contains a vector with base alignment 8 and scalar alignment 4.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpMemberDecorate %st 0 Offset 0
               OpMemberDecorate %st 1 Offset 8
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
       %vec2 = OpTypeVector %float 2
        %st  = OpTypeStruct %vec2 %float
          %S = OpTypeStruct %float %st
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(
    ValidateDecorations,
    BlockLayoutPermitsFieldsInBaseAlignmentPaddingAtEndOfStructWithScalarLayoutGood) {
  // Scalar block layout permits fields in what would normally be the padding at
  // the end of a struct.
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Float64
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %st 0 Offset 0
               OpMemberDecorate %st 1 Offset 8
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 12
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
     %double = OpTypeFloat 64
         %st = OpTypeStruct %double %float
          %S = OpTypeStruct %st %float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(
    ValidateDecorations,
    BlockLayoutPermitsStraddlingVectorWithScalarLayoutOverrideRelaxBlockLayoutGood) {
  // Same as previous, but set relaxed block layout first.  Scalar layout always
  // wins.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
       %vec4 = OpTypeVector %float 4
          %S = OpTypeStruct %float %vec4
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetRelaxBlockLayout(getValidatorOptions(), true);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(
    ValidateDecorations,
    BlockLayoutPermitsStraddlingVectorWithRelaxedLayoutOverridenByScalarBlockLayoutGood) {
  // Same as previous, but set scalar block layout first.  Scalar layout always
  // wins.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
       %vec4 = OpTypeVector %float 4
          %S = OpTypeStruct %float %vec4
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetScalarBlockLayout(getValidatorOptions(), true);
  spvValidatorOptionsSetRelaxBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, BufferBlock16bitStandardStorageBufferLayout) {
  std::string spirv = R"(
             OpCapability Shader
             OpCapability StorageUniform16
             OpExtension "SPV_KHR_16bit_storage"
             OpMemoryModel Logical GLSL450
             OpEntryPoint GLCompute %main "main"
             OpExecutionMode %main LocalSize 1 1 1
             OpDecorate %f32arr ArrayStride 4
             OpDecorate %f16arr ArrayStride 2
             OpMemberDecorate %SSBO32 0 Offset 0
             OpMemberDecorate %SSBO16 0 Offset 0
             OpDecorate %SSBO32 BufferBlock
             OpDecorate %SSBO16 BufferBlock
     %void = OpTypeVoid
    %voidf = OpTypeFunction %void
      %u32 = OpTypeInt 32 0
      %i32 = OpTypeInt 32 1
      %f32 = OpTypeFloat 32
    %uvec3 = OpTypeVector %u32 3
 %c_i32_32 = OpConstant %i32 32
%c_i32_128 = OpConstant %i32 128
   %f32arr = OpTypeArray %f32 %c_i32_128
      %f16 = OpTypeFloat 16
   %f16arr = OpTypeArray %f16 %c_i32_128
   %SSBO32 = OpTypeStruct %f32arr
   %SSBO16 = OpTypeStruct %f16arr
%_ptr_Uniform_SSBO32 = OpTypePointer Uniform %SSBO32
 %varSSBO32 = OpVariable %_ptr_Uniform_SSBO32 Uniform
%_ptr_Uniform_SSBO16 = OpTypePointer Uniform %SSBO16
 %varSSBO16 = OpVariable %_ptr_Uniform_SSBO16 Uniform
     %main = OpFunction %void None %voidf
    %label = OpLabel
             OpReturn
             OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, BlockArrayExtendedAlignmentGood) {
  // For uniform buffer, Array base alignment is 16, and ArrayStride
  // must be a multiple of 16.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 16
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_PushConstant_S = OpTypePointer PushConstant %S
          %u = OpVariable %_ptr_PushConstant_S PushConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, BlockArrayBaseAlignmentBad) {
  // For uniform buffer, Array base alignment is 16.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %u = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in Uniform "
          "storage class must follow standard uniform buffer layout rules: "
          "member 1 at offset 8 is not aligned to 16"));
}

TEST_F(ValidateDecorations, BlockArrayBaseAlignmentWithRelaxedLayoutStillBad) {
  // For uniform buffer, Array base alignment is 16, and ArrayStride
  // must be a multiple of 16.  This case uses relaxed block layout.  Relaxed
  // layout only relaxes rules for vector alignment, not array alignment.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %u DescriptorSet 0
               OpDecorate %u Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %u = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  spvValidatorOptionsSetRelaxBlockLayout(getValidatorOptions(), true);
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 4 decorated as Block for variable in Uniform "
          "storage class must follow standard uniform buffer layout rules: "
          "member 1 at offset 8 is not aligned to 16"));
}

TEST_F(ValidateDecorations, BlockArrayBaseAlignmentWithVulkan1_1StillBad) {
  // Same as previous test, but with Vulkan 1.1, which includes
  // VK_KHR_relaxed_block_layout in core.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %u DescriptorSet 0
               OpDecorate %u Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %u = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 4 decorated as Block for variable in Uniform "
          "storage class must follow relaxed uniform buffer layout rules: "
          "member 1 at offset 8 is not aligned to 16"));
}

TEST_F(ValidateDecorations,
       BlockArrayBaseAlignmentWithBlockStandardLayoutGood) {
  // Same as previous test, but with VK_KHR_uniform_buffer_standard_layout
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %u DescriptorSet 0
               OpDecorate %u Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %u = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetUniformBufferStandardLayout(getValidatorOptions(),
                                                    true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, VulkanBufferBlockOnStorageBufferBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct BufferBlock

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PushConstant-06675"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("In Vulkan, BufferBlock is disallowed on variables in "
                        "the StorageBuffer storage class"));
}

TEST_F(ValidateDecorations, PushConstantArrayBaseAlignmentGood) {
  // Tests https://github.com/KhronosGroup/SPIRV-Tools/issues/1664
  // From GLSL vertex shader:
  // #version 450
  // layout(push_constant) uniform S { vec2 v; float arr[2]; } u;
  // void main() { }

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 4
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_PushConstant_S = OpTypePointer PushConstant %S
          %u = OpVariable %_ptr_PushConstant_S PushConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, PushConstantArrayBadAlignmentBad) {
  // Like the previous test, but with offset 7 instead of 8.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 4
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 7
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_PushConstant_S = OpTypePointer PushConstant %S
          %u = OpVariable %_ptr_PushConstant_S PushConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in PushConstant "
          "storage class must follow standard storage buffer layout rules: "
          "member 1 at offset 7 is not aligned to 4"));
}

TEST_F(ValidateDecorations,
       PushConstantLayoutPermitsTightVec3ScalarPackingGood) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 12
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %v3float %float
%_ptr_PushConstant_S = OpTypePointer PushConstant %S
          %B = OpVariable %_ptr_PushConstant_S PushConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations,
       PushConstantLayoutForbidsTightScalarVec3PackingBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer PushConstant %S
          %B = OpVariable %_ptr_Uniform_S PushConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in PushConstant "
          "storage class must follow standard storage buffer layout "
          "rules: member 1 at offset 4 is not aligned to 16"));
}

TEST_F(ValidateDecorations, PushConstantMissingBlockGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer PushConstant %struct
      %pc = OpVariable %ptr PushConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, VulkanPushConstantMissingBlockBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer PushConstant %struct
      %pc = OpVariable %ptr PushConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PushConstant-06675"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("PushConstant id '2' is missing Block decoration.\n"
                        "From Vulkan spec:\n"
                        "Such variables must be identified with a Block "
                        "decoration"));
}

TEST_F(ValidateDecorations, MultiplePushConstantsSingleEntryPointGood) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Fragment %1 "main"
                OpExecutionMode %1 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

           %1 = OpFunction %void None %voidfn
       %label = OpLabel
           %2 = OpAccessChain %ptr_float %pc1 %int_0
           %3 = OpLoad %float %2
           %4 = OpAccessChain %ptr_float %pc2 %int_0
           %5 = OpLoad %float %4
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations,
       VulkanMultiplePushConstantsDifferentEntryPointGood) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Vertex %1 "func1"
                OpEntryPoint Fragment %2 "func2"
                OpExecutionMode %2 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

           %1 = OpFunction %void None %voidfn
      %label1 = OpLabel
           %3 = OpAccessChain %ptr_float %pc1 %int_0
           %4 = OpLoad %float %3
                OpReturn
                OpFunctionEnd

           %2 = OpFunction %void None %voidfn
      %label2 = OpLabel
           %5 = OpAccessChain %ptr_float %pc2 %int_0
           %6 = OpLoad %float %5
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations,
       VulkanMultiplePushConstantsUnusedSingleEntryPointGood) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Fragment %1 "main"
                OpExecutionMode %1 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

           %1 = OpFunction %void None %voidfn
       %label = OpLabel
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, VulkanMultiplePushConstantsSingleEntryPointBad) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Fragment %1 "main"
                OpExecutionMode %1 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

           %1 = OpFunction %void None %voidfn
       %label = OpLabel
           %2 = OpAccessChain %ptr_float %pc1 %int_0
           %3 = OpLoad %float %2
           %4 = OpAccessChain %ptr_float %pc2 %int_0
           %5 = OpLoad %float %4
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-06674"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Entry point id '1' uses more than one PushConstant interface.\n"
          "From Vulkan spec:\n"
          "There must be no more than one push constant block "
          "statically used per shader entry point."));
}

TEST_F(ValidateDecorations,
       VulkanMultiplePushConstantsDifferentEntryPointSubFunctionGood) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Vertex %1 "func1"
                OpEntryPoint Fragment %2 "func2"
                OpExecutionMode %2 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

        %sub1 = OpFunction %void None %voidfn
  %label_sub1 = OpLabel
           %3 = OpAccessChain %ptr_float %pc1 %int_0
           %4 = OpLoad %float %3
                OpReturn
                OpFunctionEnd

        %sub2 = OpFunction %void None %voidfn
  %label_sub2 = OpLabel
           %5 = OpAccessChain %ptr_float %pc2 %int_0
           %6 = OpLoad %float %5
                OpReturn
                OpFunctionEnd

           %1 = OpFunction %void None %voidfn
      %label1 = OpLabel
       %call1 = OpFunctionCall %void %sub1
                OpReturn
                OpFunctionEnd

           %2 = OpFunction %void None %voidfn
      %label2 = OpLabel
       %call2 = OpFunctionCall %void %sub2
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations,
       VulkanMultiplePushConstantsSingleEntryPointSubFunctionBad) {
  std::string spirv = R"(
                OpCapability Shader
                OpMemoryModel Logical GLSL450
                OpEntryPoint Fragment %1 "main"
                OpExecutionMode %1 OriginUpperLeft

                OpDecorate %struct Block
                OpMemberDecorate %struct 0 Offset 0

        %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
       %float = OpTypeFloat 32
         %int = OpTypeInt 32 0
       %int_0 = OpConstant %int 0
      %struct = OpTypeStruct %float
         %ptr = OpTypePointer PushConstant %struct
   %ptr_float = OpTypePointer PushConstant %float
         %pc1 = OpVariable %ptr PushConstant
         %pc2 = OpVariable %ptr PushConstant

        %sub1 = OpFunction %void None %voidfn
  %label_sub1 = OpLabel
           %3 = OpAccessChain %ptr_float %pc1 %int_0
           %4 = OpLoad %float %3
                OpReturn
                OpFunctionEnd

        %sub2 = OpFunction %void None %voidfn
  %label_sub2 = OpLabel
           %5 = OpAccessChain %ptr_float %pc2 %int_0
           %6 = OpLoad %float %5
                OpReturn
                OpFunctionEnd

           %1 = OpFunction %void None %voidfn
      %label1 = OpLabel
       %call1 = OpFunctionCall %void %sub1
       %call2 = OpFunctionCall %void %sub2
                OpReturn
                OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-06674"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Entry point id '1' uses more than one PushConstant interface.\n"
          "From Vulkan spec:\n"
          "There must be no more than one push constant block "
          "statically used per shader entry point."));
}

TEST_F(ValidateDecorations,
       VulkanMultiplePushConstantsSingleEntryPointInterfaceBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Vertex %func1 "func1" %pc1 %pc2
            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0
    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer PushConstant %struct
%ptr_float = OpTypePointer PushConstant %float
     %pc1 = OpVariable %ptr PushConstant
     %pc2 = OpVariable %ptr PushConstant
   %func1 = OpFunction %void None %voidfn
  %label1 = OpLabel
 %access1 = OpAccessChain %ptr_float %pc1 %int_0
   %load1 = OpLoad %float %access1
            OpReturn
            OpFunctionEnd
   %func2 = OpFunction %void None %voidfn
  %label2 = OpLabel
 %access2 = OpAccessChain %ptr_float %pc2 %int_0
   %load2 = OpLoad %float %access2
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpVariable-06673"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Entry-point has more than one variable with the "
                        "PushConstant storage class in the interface"));
}

TEST_F(ValidateDecorations, VulkanUniformMissingDescriptorSetBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0
            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer Uniform %struct
%ptr_float = OpTypePointer Uniform %float
     %var = OpVariable %ptr Uniform
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpAccessChain %ptr_float %var %int_0
       %3 = OpLoad %float %2
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Uniform id '3' is missing DescriptorSet decoration.\n"
                        "From Vulkan spec:\n"
                        "These variables must have DescriptorSet and Binding "
                        "decorations specified"));
}

TEST_F(ValidateDecorations, VulkanUniformMissingBindingBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0
            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer Uniform %struct
%ptr_float = OpTypePointer Uniform %float
     %var = OpVariable %ptr Uniform
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpAccessChain %ptr_float %var %int_0
       %3 = OpLoad %float %2
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Uniform id '3' is missing Binding decoration.\n"
                        "From Vulkan spec:\n"
                        "These variables must have DescriptorSet and Binding "
                        "decorations specified"));
}

TEST_F(ValidateDecorations, VulkanUniformConstantMissingDescriptorSetBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
 %sampler = OpTypeSampler
     %ptr = OpTypePointer UniformConstant %sampler
     %var = OpVariable %ptr UniformConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpLoad %sampler %var
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("UniformConstant id '2' is missing DescriptorSet decoration.\n"
                "From Vulkan spec:\n"
                "These variables must have DescriptorSet and Binding "
                "decorations specified"));
}

TEST_F(ValidateDecorations, VulkanUniformConstantMissingBindingBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
 %sampler = OpTypeSampler
     %ptr = OpTypePointer UniformConstant %sampler
     %var = OpVariable %ptr UniformConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpLoad %sampler %var
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("UniformConstant id '2' is missing Binding decoration.\n"
                "From Vulkan spec:\n"
                "These variables must have DescriptorSet and Binding "
                "decorations specified"));
}

TEST_F(ValidateDecorations, VulkanStorageBufferMissingDescriptorSetBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer
%ptr_float = OpTypePointer StorageBuffer %float
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpAccessChain %ptr_float %var %int_0
       %3 = OpLoad %float %2
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("StorageBuffer id '3' is missing DescriptorSet decoration.\n"
                "From Vulkan spec:\n"
                "These variables must have DescriptorSet and Binding "
                "decorations specified"));
}

TEST_F(ValidateDecorations, VulkanStorageBufferMissingBindingBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer
%ptr_float = OpTypePointer StorageBuffer %float
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
       %2 = OpAccessChain %ptr_float %var %int_0
       %3 = OpLoad %float %2
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("StorageBuffer id '3' is missing Binding decoration.\n"
                        "From Vulkan spec:\n"
                        "These variables must have DescriptorSet and Binding "
                        "decorations specified"));
}

TEST_F(ValidateDecorations,
       VulkanStorageBufferMissingDescriptorSetSubFunctionBad) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer
%ptr_float = OpTypePointer StorageBuffer %float
     %int = OpTypeInt 32 0
   %int_0 = OpConstant %int 0

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
    %call = OpFunctionCall %void %2
            OpReturn
            OpFunctionEnd
       %2 = OpFunction %void None %voidfn
  %label2 = OpLabel
       %3 = OpAccessChain %ptr_float %var %int_0
       %4 = OpLoad %float %3
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-UniformConstant-06677"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("StorageBuffer id '3' is missing DescriptorSet decoration.\n"
                "From Vulkan spec:\n"
                "These variables must have DescriptorSet and Binding "
                "decorations specified"));
}

TEST_F(ValidateDecorations,
       VulkanStorageBufferMissingDescriptorAndBindingUnusedGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft
            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
}

TEST_F(ValidateDecorations, UniformMissingDescriptorSetGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0
            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer Uniform %struct
     %var = OpVariable %ptr Uniform

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, UniformMissingBindingGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpMemberDecorate %struct 0 Offset 0
            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer Uniform %struct
     %var = OpVariable %ptr Uniform

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, UniformConstantMissingDescriptorSetGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
 %sampler = OpTypeSampler
     %ptr = OpTypePointer UniformConstant %sampler
     %var = OpVariable %ptr UniformConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, UniformConstantMissingBindingGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
 %sampler = OpTypeSampler
     %ptr = OpTypePointer UniformConstant %sampler
     %var = OpVariable %ptr UniformConstant

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, StorageBufferMissingDescriptorSetGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct BufferBlock
            OpDecorate %var Binding 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, StorageBufferMissingBindingGood) {
  std::string spirv = R"(
            OpCapability Shader
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct BufferBlock
            OpDecorate %var DescriptorSet 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float
     %ptr = OpTypePointer StorageBuffer %struct
     %var = OpVariable %ptr StorageBuffer

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, StorageBufferStorageClassArrayBaseAlignmentGood) {
  // Spot check buffer rules when using StorageBuffer storage class with Block
  // decoration.
  std::string spirv = R"(
               OpCapability Shader
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 4
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %S Block
               OpDecorate %u DescriptorSet 0
               OpDecorate %u Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer StorageBuffer %S
          %u = OpVariable %_ptr_Uniform_S StorageBuffer
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, StorageBufferStorageClassArrayBadAlignmentBad) {
  // Like the previous test, but with offset 7.
  std::string spirv = R"(
               OpCapability Shader
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpDecorate %_arr_float_uint_2 ArrayStride 4
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 7
               OpDecorate %S Block
               OpDecorate %u DescriptorSet 0
               OpDecorate %u Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
          %S = OpTypeStruct %v2float %_arr_float_uint_2
%_ptr_Uniform_S = OpTypePointer StorageBuffer %S
          %u = OpVariable %_ptr_Uniform_S StorageBuffer
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in StorageBuffer "
          "storage class must follow standard storage buffer layout rules: "
          "member 1 at offset 7 is not aligned to 4"));
}

TEST_F(ValidateDecorations, BufferBlockStandardStorageBufferLayout) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %F 0 Offset 0
               OpMemberDecorate %F 1 Offset 8
               OpDecorate %_arr_float_uint_2 ArrayStride 4
               OpDecorate %_arr_mat3v3float_uint_2 ArrayStride 48
               OpMemberDecorate %O 0 Offset 0
               OpMemberDecorate %O 1 Offset 16
               OpMemberDecorate %O 2 Offset 24
               OpMemberDecorate %O 3 Offset 32
               OpMemberDecorate %O 4 ColMajor
               OpMemberDecorate %O 4 Offset 48
               OpMemberDecorate %O 4 MatrixStride 16
               OpDecorate %_arr_O_uint_2 ArrayStride 144
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpMemberDecorate %Output 2 Offset 16
               OpMemberDecorate %Output 3 Offset 32
               OpMemberDecorate %Output 4 Offset 48
               OpMemberDecorate %Output 5 Offset 52
               OpMemberDecorate %Output 6 ColMajor
               OpMemberDecorate %Output 6 Offset 64
               OpMemberDecorate %Output 6 MatrixStride 16
               OpMemberDecorate %Output 7 Offset 96
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
          %F = OpTypeStruct %int %v2uint
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%mat2v3float = OpTypeMatrix %v3float 2
     %v3uint = OpTypeVector %uint 3
%mat3v3float = OpTypeMatrix %v3float 3
%_arr_mat3v3float_uint_2 = OpTypeArray %mat3v3float %uint_2
          %O = OpTypeStruct %v3uint %v2float %_arr_float_uint_2 %v2float %_arr_mat3v3float_uint_2
%_arr_O_uint_2 = OpTypeArray %O %uint_2
     %Output = OpTypeStruct %float %v2float %v3float %F %float %_arr_float_uint_2 %mat2v3float %_arr_O_uint_2
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations,
       StorageBufferLayoutPermitsTightVec3ScalarPackingGood) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 12
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %v3float %float
%_ptr_StorageBuffer_S = OpTypePointer StorageBuffer %S
          %B = OpVariable %_ptr_StorageBuffer_S StorageBuffer
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations,
       StorageBufferLayoutForbidsTightScalarVec3PackingBad) {
  // See https://github.com/KhronosGroup/SPIRV-Tools/issues/1666
  std::string spirv = R"(
               OpCapability Shader
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_StorageBuffer_S = OpTypePointer StorageBuffer %S
          %B = OpVariable %_ptr_StorageBuffer_S StorageBuffer
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in StorageBuffer "
          "storage class must follow standard storage buffer layout "
          "rules: member 1 at offset 4 is not aligned to 16"));
}

TEST_F(ValidateDecorations,
       BlockStandardUniformBufferLayoutIncorrectOffset0Bad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %F 0 Offset 0
               OpMemberDecorate %F 1 Offset 8
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %_arr_mat3v3float_uint_2 ArrayStride 48
               OpMemberDecorate %O 0 Offset 0
               OpMemberDecorate %O 1 Offset 16
               OpMemberDecorate %O 2 Offset 24
               OpMemberDecorate %O 3 Offset 33
               OpMemberDecorate %O 4 ColMajor
               OpMemberDecorate %O 4 Offset 80
               OpMemberDecorate %O 4 MatrixStride 16
               OpDecorate %_arr_O_uint_2 ArrayStride 176
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpMemberDecorate %Output 2 Offset 16
               OpMemberDecorate %Output 3 Offset 32
               OpMemberDecorate %Output 4 Offset 48
               OpMemberDecorate %Output 5 Offset 64
               OpMemberDecorate %Output 6 ColMajor
               OpMemberDecorate %Output 6 Offset 96
               OpMemberDecorate %Output 6 MatrixStride 16
               OpMemberDecorate %Output 7 Offset 128
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
          %F = OpTypeStruct %int %v2uint
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%mat2v3float = OpTypeMatrix %v3float 2
     %v3uint = OpTypeVector %uint 3
%mat3v3float = OpTypeMatrix %v3float 3
%_arr_mat3v3float_uint_2 = OpTypeArray %mat3v3float %uint_2
          %O = OpTypeStruct %v3uint %v2float %_arr_float_uint_2 %v2float %_arr_mat3v3float_uint_2
%_arr_O_uint_2 = OpTypeArray %O %uint_2
     %Output = OpTypeStruct %float %v2float %v3float %F %float %_arr_float_uint_2 %mat2v3float %_arr_O_uint_2
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Structure id 6 decorated as Block for variable in Uniform "
                "storage class must follow standard uniform buffer layout "
                "rules: member 2 at offset 152 is not aligned to 16"));
}

TEST_F(ValidateDecorations,
       BlockStandardUniformBufferLayoutIncorrectOffset1Bad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %F 0 Offset 0
               OpMemberDecorate %F 1 Offset 8
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %_arr_mat3v3float_uint_2 ArrayStride 48
               OpMemberDecorate %O 0 Offset 0
               OpMemberDecorate %O 1 Offset 16
               OpMemberDecorate %O 2 Offset 32
               OpMemberDecorate %O 3 Offset 64
               OpMemberDecorate %O 4 ColMajor
               OpMemberDecorate %O 4 Offset 80
               OpMemberDecorate %O 4 MatrixStride 16
               OpDecorate %_arr_O_uint_2 ArrayStride 176
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpMemberDecorate %Output 2 Offset 16
               OpMemberDecorate %Output 3 Offset 32
               OpMemberDecorate %Output 4 Offset 48
               OpMemberDecorate %Output 5 Offset 71
               OpMemberDecorate %Output 6 ColMajor
               OpMemberDecorate %Output 6 Offset 96
               OpMemberDecorate %Output 6 MatrixStride 16
               OpMemberDecorate %Output 7 Offset 128
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
          %F = OpTypeStruct %int %v2uint
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%mat2v3float = OpTypeMatrix %v3float 2
     %v3uint = OpTypeVector %uint 3
%mat3v3float = OpTypeMatrix %v3float 3
%_arr_mat3v3float_uint_2 = OpTypeArray %mat3v3float %uint_2
          %O = OpTypeStruct %v3uint %v2float %_arr_float_uint_2 %v2float %_arr_mat3v3float_uint_2
%_arr_O_uint_2 = OpTypeArray %O %uint_2
     %Output = OpTypeStruct %float %v2float %v3float %F %float %_arr_float_uint_2 %mat2v3float %_arr_O_uint_2
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Structure id 8 decorated as Block for variable in Uniform "
                "storage class must follow standard uniform buffer layout "
                "rules: member 5 at offset 71 is not aligned to 16"));
}

TEST_F(ValidateDecorations, BlockUniformBufferLayoutIncorrectArrayStrideBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %F 0 Offset 0
               OpMemberDecorate %F 1 Offset 8
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpDecorate %_arr_mat3v3float_uint_2 ArrayStride 49
               OpMemberDecorate %O 0 Offset 0
               OpMemberDecorate %O 1 Offset 16
               OpMemberDecorate %O 2 Offset 32
               OpMemberDecorate %O 3 Offset 64
               OpMemberDecorate %O 4 ColMajor
               OpMemberDecorate %O 4 Offset 80
               OpMemberDecorate %O 4 MatrixStride 16
               OpDecorate %_arr_O_uint_2 ArrayStride 176
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpMemberDecorate %Output 2 Offset 16
               OpMemberDecorate %Output 3 Offset 32
               OpMemberDecorate %Output 4 Offset 48
               OpMemberDecorate %Output 5 Offset 64
               OpMemberDecorate %Output 6 ColMajor
               OpMemberDecorate %Output 6 Offset 96
               OpMemberDecorate %Output 6 MatrixStride 16
               OpMemberDecorate %Output 7 Offset 128
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
          %F = OpTypeStruct %int %v2uint
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
%mat2v3float = OpTypeMatrix %v3float 2
     %v3uint = OpTypeVector %uint 3
%mat3v3float = OpTypeMatrix %v3float 3
%_arr_mat3v3float_uint_2 = OpTypeArray %mat3v3float %uint_2
          %O = OpTypeStruct %v3uint %v2float %_arr_float_uint_2 %v2float %_arr_mat3v3float_uint_2
%_arr_O_uint_2 = OpTypeArray %O %uint_2
     %Output = OpTypeStruct %float %v2float %v3float %F %float %_arr_float_uint_2 %mat2v3float %_arr_O_uint_2
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 6 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 4 "
          "contains "
          "an array with stride 49 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations,
       BufferBlockStandardStorageBufferLayoutImproperStraddleBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 8
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
     %Output = OpTypeStruct %float %v3float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Structure id 3 decorated as BufferBlock for variable in "
                "Uniform storage class must follow standard storage buffer "
                "layout rules: member 1 at offset 8 is not aligned to 16"));
}

TEST_F(ValidateDecorations,
       BlockUniformBufferLayoutOffsetInsideArrayPaddingBad) {
  // In this case the 2nd member fits entirely within the padding.
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpDecorate %_arr_float_uint_2 ArrayStride 16
               OpMemberDecorate %Output 0 Offset 0
               OpMemberDecorate %Output 1 Offset 20
               OpDecorate %Output Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
     %uint_2 = OpConstant %uint 2
%_arr_float_uint_2 = OpTypeArray %float %uint_2
     %Output = OpTypeStruct %_arr_float_uint_2 %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 4 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 1 at "
          "offset 20 overlaps previous member ending at offset 31"));
}

TEST_F(ValidateDecorations,
       BlockUniformBufferLayoutOffsetInsideStructPaddingBad) {
  // In this case the 2nd member fits entirely within the padding.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main"
               OpExecutionMode %1 LocalSize 1 1 1
               OpMemberDecorate %_struct_6 0 Offset 0
               OpMemberDecorate %_struct_2 0 Offset 0
               OpMemberDecorate %_struct_2 1 Offset 4
               OpDecorate %_struct_2 Block
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
      %float = OpTypeFloat 32
  %_struct_6 = OpTypeStruct %float
  %_struct_2 = OpTypeStruct %_struct_6 %float
%_ptr_Uniform__struct_2 = OpTypePointer Uniform %_struct_2
          %8 = OpVariable %_ptr_Uniform__struct_2 Uniform
          %1 = OpFunction %void None %4
          %9 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 1 at "
          "offset 4 overlaps previous member ending at offset 15"));
}

TEST_F(ValidateDecorations, BlockLayoutOffsetOutOfOrderGoodUniversal1_0) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %Outer 0 Offset 4
               OpMemberDecorate %Outer 1 Offset 0
               OpDecorate %Outer Block
               OpDecorate %O DescriptorSet 0
               OpDecorate %O Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
      %Outer = OpTypeStruct %uint %uint
%_ptr_Uniform_Outer = OpTypePointer Uniform %Outer
          %O = OpVariable %_ptr_Uniform_Outer Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_0));
}

TEST_F(ValidateDecorations, BlockLayoutOffsetOutOfOrderGoodOpenGL4_5) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %Outer 0 Offset 4
               OpMemberDecorate %Outer 1 Offset 0
               OpDecorate %Outer Block
               OpDecorate %O DescriptorSet 0
               OpDecorate %O Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
      %Outer = OpTypeStruct %uint %uint
%_ptr_Uniform_Outer = OpTypePointer Uniform %Outer
          %O = OpVariable %_ptr_Uniform_Outer Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_OPENGL_4_5));
}

TEST_F(ValidateDecorations, BlockLayoutOffsetOutOfOrderGoodVulkan1_1) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %Outer 0 Offset 4
               OpMemberDecorate %Outer 1 Offset 0
               OpDecorate %Outer Block
               OpDecorate %O DescriptorSet 0
               OpDecorate %O Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
      %Outer = OpTypeStruct %uint %uint
%_ptr_Uniform_Outer = OpTypePointer Uniform %Outer
          %O = OpVariable %_ptr_Uniform_Outer Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1))
      << getDiagnosticString();
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, BlockLayoutOffsetOverlapBad) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %Outer 0 Offset 0
               OpMemberDecorate %Outer 1 Offset 16
               OpMemberDecorate %Inner 0 Offset 0
               OpMemberDecorate %Inner 1 Offset 16
               OpDecorate %Outer Block
               OpDecorate %O DescriptorSet 0
               OpDecorate %O Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
      %Inner = OpTypeStruct %uint %uint
      %Outer = OpTypeStruct %Inner %uint
%_ptr_Uniform_Outer = OpTypePointer Uniform %Outer
          %O = OpVariable %_ptr_Uniform_Outer Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 1 at "
          "offset 16 overlaps previous member ending at offset 31"));
}

TEST_F(ValidateDecorations, BufferBlockEmptyStruct) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 430
               OpMemberDecorate %Output 0 Offset 0
               OpDecorate %Output BufferBlock
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
          %S = OpTypeStruct
     %Output = OpTypeStruct %S
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
 %dataOutput = OpVariable %_ptr_Uniform_Output Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, RowMajorMatrixTightPackingGood) {
  // Row major matrix rule:
  //     A row-major matrix of C columns has a base alignment equal to
  //     the base alignment of a vector of C matrix components.
  // Note: The "matrix component" is the scalar element type.

  // The matrix has 3 columns and 2 rows (C=3, R=2).
  // So the base alignment of b is the same as a vector of 3 floats, which is 16
  // bytes. The matrix consists of two of these, and therefore occupies 2 x 16
  // bytes, or 32 bytes.
  //
  // So the offsets can be:
  // a -> 0
  // b -> 16
  // c -> 48
  // d -> 60 ; d fits at bytes 12-15 after offset of c. Tight (vec3;float)
  // packing

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpSource GLSL 450
               OpMemberDecorate %_struct_2 0 Offset 0
               OpMemberDecorate %_struct_2 1 RowMajor
               OpMemberDecorate %_struct_2 1 Offset 16
               OpMemberDecorate %_struct_2 1 MatrixStride 16
               OpMemberDecorate %_struct_2 2 Offset 48
               OpMemberDecorate %_struct_2 3 Offset 60
               OpDecorate %_struct_2 Block
               OpDecorate %3 DescriptorSet 0
               OpDecorate %3 Binding 0
       %void = OpTypeVoid
          %5 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v2float = OpTypeVector %float 2
%mat3v2float = OpTypeMatrix %v2float 3
    %v3float = OpTypeVector %float 3
  %_struct_2 = OpTypeStruct %v4float %mat3v2float %v3float %float
%_ptr_Uniform__struct_2 = OpTypePointer Uniform %_struct_2
          %3 = OpVariable %_ptr_Uniform__struct_2 Uniform
          %1 = OpFunction %void None %5
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState())
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, ArrayArrayRowMajorMatrixTightPackingGood) {
  // Like the previous case, but we have an array of arrays of matrices.
  // The RowMajor decoration goes on the struct member (surprisingly).

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpSource GLSL 450
               OpMemberDecorate %_struct_2 0 Offset 0
               OpMemberDecorate %_struct_2 1 RowMajor
               OpMemberDecorate %_struct_2 1 Offset 16
               OpMemberDecorate %_struct_2 1 MatrixStride 16
               OpMemberDecorate %_struct_2 2 Offset 80
               OpMemberDecorate %_struct_2 3 Offset 92
               OpDecorate %arr_mat ArrayStride 32
               OpDecorate %arr_arr_mat ArrayStride 32
               OpDecorate %_struct_2 Block
               OpDecorate %3 DescriptorSet 0
               OpDecorate %3 Binding 0
       %void = OpTypeVoid
          %5 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v2float = OpTypeVector %float 2
%mat3v2float = OpTypeMatrix %v2float 3
%uint        = OpTypeInt 32 0
%uint_1      = OpConstant %uint 1
%uint_2      = OpConstant %uint 2
    %arr_mat = OpTypeArray %mat3v2float %uint_1
%arr_arr_mat = OpTypeArray %arr_mat %uint_2
    %v3float = OpTypeVector %float 3
  %_struct_2 = OpTypeStruct %v4float %arr_arr_mat %v3float %float
%_ptr_Uniform__struct_2 = OpTypePointer Uniform %_struct_2
          %3 = OpVariable %_ptr_Uniform__struct_2 Uniform
          %1 = OpFunction %void None %5
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, ArrayArrayRowMajorMatrixNextMemberOverlapsBad) {
  // Like the previous case, but the offset of member 2 overlaps the matrix.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpSource GLSL 450
               OpMemberDecorate %_struct_2 0 Offset 0
               OpMemberDecorate %_struct_2 1 RowMajor
               OpMemberDecorate %_struct_2 1 Offset 16
               OpMemberDecorate %_struct_2 1 MatrixStride 16
               OpMemberDecorate %_struct_2 2 Offset 64
               OpMemberDecorate %_struct_2 3 Offset 92
               OpDecorate %arr_mat ArrayStride 32
               OpDecorate %arr_arr_mat ArrayStride 32
               OpDecorate %_struct_2 Block
               OpDecorate %3 DescriptorSet 0
               OpDecorate %3 Binding 0
       %void = OpTypeVoid
          %5 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v2float = OpTypeVector %float 2
%mat3v2float = OpTypeMatrix %v2float 3
%uint        = OpTypeInt 32 0
%uint_1      = OpConstant %uint 1
%uint_2      = OpConstant %uint 2
    %arr_mat = OpTypeArray %mat3v2float %uint_1
%arr_arr_mat = OpTypeArray %arr_mat %uint_2
    %v3float = OpTypeVector %float 3
  %_struct_2 = OpTypeStruct %v4float %arr_arr_mat %v3float %float
%_ptr_Uniform__struct_2 = OpTypePointer Uniform %_struct_2
          %3 = OpVariable %_ptr_Uniform__struct_2 Uniform
          %1 = OpFunction %void None %5
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 2 at "
          "offset 64 overlaps previous member ending at offset 79"));
}

TEST_F(ValidateDecorations, StorageBufferArraySizeCalculationPackGood) {
  // Original GLSL

  // #version 450
  // layout (set=0,binding=0) buffer S {
  //   uvec3 arr[2][2]; // first 3 elements are 16 bytes, last is 12
  //   uint i;  // Can't have offset 60 = 3x16 + 12
  // } B;
  // void main() {}

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
               OpDecorate %_arr__arr_v3uint_uint_2_uint_2 ArrayStride 32
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 64
               OpDecorate %_struct_4 BufferBlock
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
       %void = OpTypeVoid
          %7 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
     %uint_2 = OpConstant %uint 2
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_arr__arr_v3uint_uint_2_uint_2 = OpTypeArray %_arr_v3uint_uint_2 %uint_2
  %_struct_4 = OpTypeStruct %_arr__arr_v3uint_uint_2_uint_2 %uint
%_ptr_Uniform__struct_4 = OpTypePointer Uniform %_struct_4
          %5 = OpVariable %_ptr_Uniform__struct_4 Uniform
          %1 = OpFunction %void None %7
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, StorageBufferArraySizeCalculationPackGoodScalar) {
  // Original GLSL

  // #version 450
  // layout (set=0,binding=0) buffer S {
  //   uvec3 arr[2][2]; // first 3 elements are 16 bytes, last is 12
  //   uint i;  // Can have offset 60 = 3x16 + 12
  // } B;
  // void main() {}

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
               OpDecorate %_arr__arr_v3uint_uint_2_uint_2 ArrayStride 32
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 60
               OpDecorate %_struct_4 BufferBlock
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
       %void = OpTypeVoid
          %7 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
     %uint_2 = OpConstant %uint 2
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_arr__arr_v3uint_uint_2_uint_2 = OpTypeArray %_arr_v3uint_uint_2 %uint_2
  %_struct_4 = OpTypeStruct %_arr__arr_v3uint_uint_2_uint_2 %uint
%_ptr_Uniform__struct_4 = OpTypePointer Uniform %_struct_4
          %5 = OpVariable %_ptr_Uniform__struct_4 Uniform
          %1 = OpFunction %void None %7
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  options_->scalar_block_layout = true;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, StorageBufferArraySizeCalculationPackBad) {
  // Like previous but, the offset of the second member is too small.

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
               OpDecorate %_arr__arr_v3uint_uint_2_uint_2 ArrayStride 32
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 60
               OpDecorate %_struct_4 BufferBlock
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
       %void = OpTypeVoid
          %7 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
     %uint_2 = OpConstant %uint 2
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_arr__arr_v3uint_uint_2_uint_2 = OpTypeArray %_arr_v3uint_uint_2 %uint_2
  %_struct_4 = OpTypeStruct %_arr__arr_v3uint_uint_2_uint_2 %uint
%_ptr_Uniform__struct_4 = OpTypePointer Uniform %_struct_4
          %5 = OpVariable %_ptr_Uniform__struct_4 Uniform
          %1 = OpFunction %void None %7
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 4 decorated as BufferBlock for variable "
                        "in Uniform storage class must follow standard storage "
                        "buffer layout rules: member 1 at offset 60 overlaps "
                        "previous member ending at offset 63"));
}

TEST_F(ValidateDecorations, UniformBufferArraySizeCalculationPackGood) {
  // Like the corresponding buffer block case, but the array padding must
  // count for the last element as well, and so the offset of the second
  // member must be at least 64.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
               OpDecorate %_arr__arr_v3uint_uint_2_uint_2 ArrayStride 32
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 64
               OpDecorate %_struct_4 Block
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
       %void = OpTypeVoid
          %7 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
     %uint_2 = OpConstant %uint 2
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_arr__arr_v3uint_uint_2_uint_2 = OpTypeArray %_arr_v3uint_uint_2 %uint_2
  %_struct_4 = OpTypeStruct %_arr__arr_v3uint_uint_2_uint_2 %uint
%_ptr_Uniform__struct_4 = OpTypePointer Uniform %_struct_4
          %5 = OpVariable %_ptr_Uniform__struct_4 Uniform
          %1 = OpFunction %void None %7
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, UniformBufferArraySizeCalculationPackBad) {
  // Like previous but, the offset of the second member is too small.

  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main"
               OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
               OpDecorate %_arr__arr_v3uint_uint_2_uint_2 ArrayStride 32
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 60
               OpDecorate %_struct_4 Block
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
       %void = OpTypeVoid
          %7 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
     %uint_2 = OpConstant %uint 2
%_arr_v3uint_uint_2 = OpTypeArray %v3uint %uint_2
%_arr__arr_v3uint_uint_2_uint_2 = OpTypeArray %_arr_v3uint_uint_2 %uint_2
  %_struct_4 = OpTypeStruct %_arr__arr_v3uint_uint_2_uint_2 %uint
%_ptr_Uniform__struct_4 = OpTypePointer Uniform %_struct_4
          %5 = OpVariable %_ptr_Uniform__struct_4 Uniform
          %1 = OpFunction %void None %7
         %12 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 4 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 1 at "
          "offset 60 overlaps previous member ending at offset 63"));
}

TEST_F(ValidateDecorations, LayoutNotCheckedWhenSkipBlockLayout) {
  // Checks that block layout is not verified in skipping block layout mode.
  // Even for obviously wrong layout.
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 3 ; wrong alignment
               OpMemberDecorate %S 1 Offset 3 ; same offset as before!
               OpDecorate %S Block
               OpDecorate %B DescriptorSet 0
               OpDecorate %B Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float
%_ptr_Uniform_S = OpTypePointer Uniform %S
          %B = OpVariable %_ptr_Uniform_S Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  spvValidatorOptionsSetSkipBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, EntryPointVariableWrongStorageClass) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %1 "func" %var
OpExecutionMode %1 OriginUpperLeft
%void = OpTypeVoid
%int = OpTypeInt 32 0
%ptr_int_Workgroup = OpTypePointer Workgroup %int
%var = OpVariable %ptr_int_Workgroup Workgroup
%func_ty = OpTypeFunction %void
%1 = OpFunction %void None %func_ty
%2 = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpEntryPoint interfaces must be OpVariables with "
                        "Storage Class of Input(1) or Output(3). Found Storage "
                        "Class 4 for Entry Point id 1."));
}

TEST_F(ValidateDecorations, VulkanMemoryModelNonCoherent) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability VulkanMemoryModelKHR
OpCapability Linkage
OpExtension "SPV_KHR_vulkan_memory_model"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical VulkanKHR
OpDecorate %1 Coherent
%2 = OpTypeInt 32 0
%3 = OpTypePointer StorageBuffer %2
%1 = OpVariable %3 StorageBuffer
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Coherent decoration targeting '1[%1]' is "
                        "banned when using the Vulkan memory model."));
}

TEST_F(ValidateDecorations, VulkanMemoryModelNoCoherentMember) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability VulkanMemoryModelKHR
OpCapability Linkage
OpExtension "SPV_KHR_vulkan_memory_model"
OpMemoryModel Logical VulkanKHR
OpMemberDecorate %1 0 Coherent
%2 = OpTypeInt 32 0
%1 = OpTypeStruct %2 %2
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Coherent decoration targeting '1[%_struct_1]' (member index 0) "
          "is banned when using the Vulkan memory model."));
}

TEST_F(ValidateDecorations, VulkanMemoryModelNoVolatile) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability VulkanMemoryModelKHR
OpCapability Linkage
OpExtension "SPV_KHR_vulkan_memory_model"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical VulkanKHR
OpDecorate %1 Volatile
%2 = OpTypeInt 32 0
%3 = OpTypePointer StorageBuffer %2
%1 = OpVariable %3 StorageBuffer
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Volatile decoration targeting '1[%1]' is banned when "
                        "using the Vulkan memory model."));
}

TEST_F(ValidateDecorations, VulkanMemoryModelNoVolatileMember) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability VulkanMemoryModelKHR
OpCapability Linkage
OpExtension "SPV_KHR_vulkan_memory_model"
OpMemoryModel Logical VulkanKHR
OpMemberDecorate %1 1 Volatile
%2 = OpTypeInt 32 0
%1 = OpTypeStruct %2 %2
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Volatile decoration targeting '1[%_struct_1]' (member "
                        "index 1) is banned when using the Vulkan memory "
                        "model."));
}

TEST_F(ValidateDecorations, FPRoundingModeGood) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%float_1_25 = OpConstant %float 1.25
%half_ptr = OpTypePointer StorageBuffer %half
%half_ptr_var = OpVariable %half_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %half %float_1_25
OpStore %half_ptr_var %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FPRoundingModeVectorGood) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%v2half = OpTypeVector %half 2
%v2float = OpTypeVector %float 2
%float_1_25 = OpConstant %float 1.25
%floats = OpConstantComposite %v2float %float_1_25 %float_1_25
%halfs_ptr = OpTypePointer StorageBuffer %v2half
%halfs_ptr_var = OpVariable %halfs_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %v2half %floats
OpStore %halfs_ptr_var %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FPRoundingModeNotOpFConvert) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%short = OpTypeInt 16 1
%int = OpTypeInt 32 1
%int_17 = OpConstant %int 17
%short_ptr = OpTypePointer StorageBuffer %short
%short_ptr_var = OpVariable %short_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpSConvert %short %int_17
OpStore %short_ptr_var %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("FPRoundingMode decoration can be applied only to a "
                        "width-only conversion instruction for floating-point "
                        "object."));
}

TEST_F(ValidateDecorations, FPRoundingModeNoOpStoreGood) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%float_1_25 = OpConstant %float 1.25
%half_ptr = OpTypePointer StorageBuffer %half
%half_ptr_var = OpVariable %half_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %half %float_1_25
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FPRoundingModeFConvert64to16Good) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpCapability Float64
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%double = OpTypeFloat 64
%double_1_25 = OpConstant %double 1.25
%half_ptr = OpTypePointer StorageBuffer %half
%half_ptr_var = OpVariable %half_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %half %double_1_25
OpStore %half_ptr_var %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FPRoundingModeNotStoreInFloat16) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpCapability Float64
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%float = OpTypeFloat 32
%double = OpTypeFloat 64
%double_1_25 = OpConstant %double 1.25
%float_ptr = OpTypePointer StorageBuffer %float
%float_ptr_var = OpVariable %float_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %float %double_1_25
OpStore %float_ptr_var %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("FPRoundingMode decoration can be applied only to the "
                "Object operand of an OpStore storing through a "
                "pointer to a 16-bit floating-point scalar or vector object."));
}

TEST_F(ValidateDecorations, FPRoundingModeMultipleOpStoreGood) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%float_1_25 = OpConstant %float 1.25
%half_ptr = OpTypePointer StorageBuffer %half
%half_ptr_var_0 = OpVariable %half_ptr StorageBuffer
%half_ptr_var_1 = OpVariable %half_ptr StorageBuffer
%half_ptr_var_2 = OpVariable %half_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %half %float_1_25
OpStore %half_ptr_var_0 %_
OpStore %half_ptr_var_1 %_
OpStore %half_ptr_var_2 %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, FPRoundingModeMultipleUsesBad) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%float_1_25 = OpConstant %float 1.25
%half_ptr = OpTypePointer StorageBuffer %half
%half_ptr_var_0 = OpVariable %half_ptr StorageBuffer
%half_ptr_var_1 = OpVariable %half_ptr StorageBuffer
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%_ = OpFConvert %half %float_1_25
OpStore %half_ptr_var_0 %_
%result = OpFAdd %half %_ %_
OpStore %half_ptr_var_1 %_
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("FPRoundingMode decoration can be applied only to the "
                        "Object operand of an OpStore."));
}

TEST_F(ValidateDecorations, VulkanFPRoundingModeGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability StorageBuffer16BitAccess
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %ssbo 0 Offset 0
               OpDecorate %ssbo Block
               OpDecorate %_ DescriptorSet 0
               OpDecorate %_ Binding 0
               OpDecorate %17 FPRoundingMode RTE
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
    %float_1 = OpConstant %float 1
       %half = OpTypeFloat 16
       %ssbo = OpTypeStruct %half
%_ptr_StorageBuffer_ssbo = OpTypePointer StorageBuffer %ssbo
          %_ = OpVariable %_ptr_StorageBuffer_ssbo StorageBuffer
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_StorageBuffer_half = OpTypePointer StorageBuffer %half
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_float Function
               OpStore %b %float_1
         %16 = OpLoad %float %b
         %17 = OpFConvert %half %16
         %19 = OpAccessChain %_ptr_StorageBuffer_half %_ %int_0
               OpStore %19 %17
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateDecorations, VulkanFPRoundingModeBadMode) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability StorageBuffer16BitAccess
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %ssbo 0 Offset 0
               OpDecorate %ssbo Block
               OpDecorate %_ DescriptorSet 0
               OpDecorate %_ Binding 0
               OpDecorate %17 FPRoundingMode RTP
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
    %float_1 = OpConstant %float 1
       %half = OpTypeFloat 16
       %ssbo = OpTypeStruct %half
%_ptr_StorageBuffer_ssbo = OpTypePointer StorageBuffer %ssbo
          %_ = OpVariable %_ptr_StorageBuffer_ssbo StorageBuffer
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_StorageBuffer_half = OpTypePointer StorageBuffer %half
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_float Function
               OpStore %b %float_1
         %16 = OpLoad %float %b
         %17 = OpFConvert %half %16
         %19 = OpAccessChain %_ptr_StorageBuffer_half %_ %int_0
               OpStore %19 %17
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-FPRoundingMode-04675"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("In Vulkan, the FPRoundingMode mode must only by RTE or RTZ."));
}

TEST_F(ValidateDecorations, GroupDecorateTargetsDecorationGroup) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
%1 = OpDecorationGroup
OpGroupDecorate %1 %1
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpGroupDecorate may not target OpDecorationGroup <id> "
                        "'1[%1]'"));
}

TEST_F(ValidateDecorations, GroupDecorateTargetsDecorationGroup2) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
%1 = OpDecorationGroup
OpGroupDecorate %1 %2 %1
%2 = OpTypeVoid
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpGroupDecorate may not target OpDecorationGroup <id> "
                        "'1[%1]'"));
}

TEST_F(ValidateDecorations, RecurseThroughRuntimeArray) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %outer Block
OpMemberDecorate %inner 0 Offset 0
OpMemberDecorate %inner 1 Offset 1
OpDecorate %runtime ArrayStride 16
OpMemberDecorate %outer 0 Offset 0
%int = OpTypeInt 32 0
%inner = OpTypeStruct %int %int
%runtime = OpTypeRuntimeArray %inner
%outer = OpTypeStruct %runtime
%outer_ptr = OpTypePointer StorageBuffer %outer
%var = OpVariable %outer_ptr StorageBuffer
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 3 decorated as Block for variable in StorageBuffer "
          "storage class must follow standard storage buffer layout "
          "rules: member 1 at offset 1 is not aligned to 4"));
}

TEST_F(ValidateDecorations, VulkanStructWithoutDecorationWithRuntimeArray) {
  std::string str = R"(
              OpCapability Shader
              OpMemoryModel Logical GLSL450
              OpEntryPoint Fragment %func "func"
              OpExecutionMode %func OriginUpperLeft
              OpDecorate %array_t ArrayStride 4
              OpMemberDecorate %struct_t 0 Offset 0
              OpMemberDecorate %struct_t 1 Offset 4
     %uint_t = OpTypeInt 32 0
   %array_t = OpTypeRuntimeArray %uint_t
  %struct_t = OpTypeStruct %uint_t %array_t
%struct_ptr = OpTypePointer StorageBuffer %struct_t
         %2 = OpVariable %struct_ptr StorageBuffer
      %void = OpTypeVoid
    %func_t = OpTypeFunction %void
      %func = OpFunction %void None %func_t
         %1 = OpLabel
              OpReturn
              OpFunctionEnd
)";

  CompileSuccessfully(str.c_str(), SPV_ENV_VULKAN_1_1);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpTypeRuntimeArray-04680"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Vulkan, OpTypeStruct containing an OpTypeRuntimeArray "
                        "must be decorated with Block or BufferBlock."));
}

TEST_F(ValidateDecorations, EmptyStructAtNonZeroOffsetGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpMemberDecorate %struct 1 Offset 16
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%empty = OpTypeStruct
%struct = OpTypeStruct %float %empty
%ptr_struct_ubo = OpTypePointer Uniform %struct
%var = OpVariable %ptr_struct_ubo Uniform
%voidfn = OpTypeFunction %void
%main = OpFunction %void None %voidfn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

// OpDecorateId

TEST_F(ValidateDecorations, DecorateIdGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %subgroupscope "subgroupscope"
OpName %int0 "int0"
OpName %fn "fn"
OpDecorateId %int0 UniformId %subgroupscope
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%subgroupscope = OpConstant %int 3
%int0 = OpConstantNull %int
%fn = OpTypeFunction %void
%main = OpFunction %void None %fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, DecorateIdGroupBad) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %subgroupscope "subgroupscope"
OpName %int0 "int0"
OpName %fn "fn"
OpName %group "group"
OpDecorateId %group UniformId %subgroupscope
%group = OpDecorationGroup
OpGroupDecorate %group %int0
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%subgroupscope = OpConstant %int 3
%int0 = OpConstantNull %int
%fn = OpTypeFunction %void
%main = OpFunction %void None %fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must not be an OpDecorationGroup instruction.\n"
                        "  OpDecorateId %group UniformId %subgroupscope"));
}

TEST_F(ValidateDecorations, DecorateIdOutOfOrderBad) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %subgroupscope "subgroupscope"
OpName %int0 "int0"
OpName %fn "fn"
OpDecorateId %int0 UniformId %subgroupscope
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%int0 = OpConstantNull %int
%subgroupscope = OpConstant %int 3
%fn = OpTypeFunction %void
%main = OpFunction %void None %fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("[%subgroupscope]' must appear earlier in the"
                        " binary than the target\n"
                        "  OpDecorateId %int0 UniformId %subgroupscope"));
}

// Uniform and UniformId decorations

TEST_F(ValidateDecorations, UniformDecorationGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %int0 Uniform
OpDecorate %var Uniform
OpDecorate %val Uniform
%void = OpTypeVoid
%int = OpTypeInt 32 1
%int0 = OpConstantNull %int
%intptr = OpTypePointer Private %int
%var = OpVariable %intptr Private
%fn = OpTypeFunction %void
%main = OpFunction %void None %fn
%entry = OpLabel
%val = OpLoad %int %var
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

// Returns SPIR-V assembly for a shader that uses a given decoration
// instruction.
std::string ShaderWithUniformLikeDecoration(const std::string& inst) {
  return std::string(R"(
OpCapability Shader
OpMemoryModel Logical Simple
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %subgroupscope "subgroupscope"
OpName %call "call"
OpName %myfunc "myfunc"
OpName %int0 "int0"
OpName %int1 "int1"
OpName %float0 "float0"
OpName %fn "fn"
)") + inst +
         R"(
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%int1 = OpConstant %int 1
%int_99 = OpConstant %int 99
%subgroupscope = OpConstant %int 3
%float0 = OpConstantNull %float
%int0 = OpConstantNull %int
%fn = OpTypeFunction %void
%myfunc = OpFunction %void None %fn
%myfuncentry = OpLabel
OpReturn
OpFunctionEnd
%main = OpFunction %void None %fn
%entry = OpLabel
%call = OpFunctionCall %void %myfunc
OpReturn
OpFunctionEnd
)";
}

TEST_F(ValidateDecorations, UniformIdDecorationWithScopeIdV13Bad) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorateId %int0 UniformId %subgroupscope");
  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_WRONG_VERSION,
            ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires SPIR-V version 1.4 or later\n"
                        "  OpDecorateId %int0 UniformId %subgroupscope"))
      << spirv;
}

TEST_F(ValidateDecorations, UniformIdDecorationWithScopeIdV13BadTargetV14) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorateId %int0 UniformId %subgroupscope");
  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_WRONG_VERSION,
            ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires SPIR-V version 1.4 or later"));
}

TEST_F(ValidateDecorations, UniformIdDecorationWithScopeIdV14Good) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorateId %int0 UniformId %subgroupscope");
  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, UniformDecorationTargetsTypeBad) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorate %fn Uniform");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Uniform decoration applied to a non-object"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("%fn = OpTypeFunction %void"));
}

TEST_F(ValidateDecorations, UniformIdDecorationTargetsTypeBad) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorateId %fn UniformId %subgroupscope");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("UniformId decoration applied to a non-object"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("%fn = OpTypeFunction %void"));
}

TEST_F(ValidateDecorations, UniformDecorationTargetsVoidValueBad) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorate %call Uniform");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Uniform decoration applied to a value with void type\n"
                        "  %call = OpFunctionCall %void %myfunc"));
}

TEST_F(ValidateDecorations, UniformIdDecorationTargetsVoidValueBad) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorateId %call UniformId %subgroupscope");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4))
      << spirv;
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("UniformId decoration applied to a value with void type\n"
                "  %call = OpFunctionCall %void %myfunc"));
}

TEST_F(ValidateDecorations,
       UniformDecorationWithScopeIdV14IdIsFloatValueIsBad) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorateId %int0 UniformId %float0");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA,
            ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ConstantNull: expected scope to be a 32-bit int"));
}

TEST_F(ValidateDecorations,
       UniformDecorationWithScopeIdV14IdIsInvalidIntValueBad) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorateId %int0 UniformId %int_99");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA,
            ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid scope value:\n %int_99 = OpConstant %int 99\n"));
}

TEST_F(ValidateDecorations, UniformDecorationWithScopeIdV14VulkanEnv) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorateId %int0 UniformId %int1");

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1_SPIRV_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA,
            ValidateInstructions(SPV_ENV_VULKAN_1_1_SPIRV_1_4));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-04636"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr(": in Vulkan environment Execution Scope is limited to "
                        "Workgroup and Subgroup"));
}

TEST_F(ValidateDecorations, UniformDecorationWithWrongInstructionBad) {
  const std::string spirv =
      ShaderWithUniformLikeDecoration("OpDecorateId %int0 Uniform");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Decorations that don't take ID parameters may not be "
                        "used with OpDecorateId\n"
                        "  OpDecorateId %int0 Uniform"));
}

TEST_F(ValidateDecorations, UniformIdDecorationWithWrongInstructionBad) {
  const std::string spirv = ShaderWithUniformLikeDecoration(
      "OpDecorate %int0 UniformId %subgroupscope");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Decorations taking ID parameters may not be used with OpDecorateId\n"
          "  OpDecorate %int0 UniformId %subgroupscope"));
}

TEST_F(ValidateDecorations, MultipleOffsetDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 Offset 0

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
  %struct = OpTypeStruct %float

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2', member '0' decorated with Offset multiple "
                        "times is not allowed."));
}

TEST_F(ValidateDecorations, MultipleArrayStrideDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %array ArrayStride 4
            OpDecorate %array ArrayStride 4

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
    %uint = OpTypeInt 32 0
  %uint_4 = OpConstant %uint 4
   %array = OpTypeArray %float %uint_4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2' decorated with ArrayStride multiple "
                        "times is not allowed."));
}

TEST_F(ValidateDecorations, MultipleMatrixStrideDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 ColMajor
            OpMemberDecorate %struct 0 MatrixStride 16
            OpMemberDecorate %struct 0 MatrixStride 16

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
   %fvec4 = OpTypeVector %float 4
   %fmat4 = OpTypeMatrix %fvec4 4
  %struct = OpTypeStruct %fmat4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2', member '0' decorated with MatrixStride "
                        "multiple times is not allowed."));
}

TEST_F(ValidateDecorations, MultipleRowMajorDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 MatrixStride 16
            OpMemberDecorate %struct 0 RowMajor
            OpMemberDecorate %struct 0 RowMajor

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
   %fvec4 = OpTypeVector %float 4
   %fmat4 = OpTypeMatrix %fvec4 4
  %struct = OpTypeStruct %fmat4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2', member '0' decorated with RowMajor multiple "
                        "times is not allowed."));
}

TEST_F(ValidateDecorations, MultipleColMajorDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 MatrixStride 16
            OpMemberDecorate %struct 0 ColMajor
            OpMemberDecorate %struct 0 ColMajor

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
   %fvec4 = OpTypeVector %float 4
   %fmat4 = OpTypeMatrix %fvec4 4
  %struct = OpTypeStruct %fmat4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2', member '0' decorated with ColMajor multiple "
                        "times is not allowed."));
}

TEST_F(ValidateDecorations, RowMajorAndColMajorDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 MatrixStride 16
            OpMemberDecorate %struct 0 ColMajor
            OpMemberDecorate %struct 0 RowMajor

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
   %fvec4 = OpTypeVector %float 4
   %fmat4 = OpTypeMatrix %fvec4 4
  %struct = OpTypeStruct %fmat4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("ID '2', member '0' decorated with both RowMajor and "
                        "ColMajor is not allowed."));
}

TEST_F(ValidateDecorations, BlockAndBufferBlockDecorationsOnSameID) {
  std::string spirv = R"(
            OpCapability Shader
            OpMemoryModel Logical GLSL450
            OpEntryPoint Fragment %1 "main"
            OpExecutionMode %1 OriginUpperLeft

            OpDecorate %struct Block
            OpDecorate %struct BufferBlock
            OpMemberDecorate %struct 0 Offset 0
            OpMemberDecorate %struct 0 MatrixStride 16
            OpMemberDecorate %struct 0 RowMajor

    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
   %float = OpTypeFloat 32
   %fvec4 = OpTypeVector %float 4
   %fmat4 = OpTypeMatrix %fvec4 4
  %struct = OpTypeStruct %fmat4

       %1 = OpFunction %void None %voidfn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "ID '2' decorated with both BufferBlock and Block is not allowed."));
}

std::string MakeIntegerShader(
    const std::string& decoration, const std::string& inst,
    const std::string& extension =
        "OpExtension \"SPV_KHR_no_integer_wrap_decoration\"") {
  return R"(
OpCapability Shader
OpCapability Linkage
)" + extension +
         R"(
%glsl = OpExtInstImport "GLSL.std.450"
%opencl = OpExtInstImport "OpenCL.std"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpName %entry "entry"
)" + decoration +
         R"(
    %void = OpTypeVoid
  %voidfn = OpTypeFunction %void
     %int = OpTypeInt 32 1
    %zero = OpConstantNull %int
   %float = OpTypeFloat 32
  %float0 = OpConstantNull %float
    %main = OpFunction %void None %voidfn
   %entry = OpLabel
)" + inst +
         R"(
OpReturn
OpFunctionEnd)";
}

// NoSignedWrap

TEST_F(ValidateDecorations, NoSignedWrapOnTypeBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %void NoSignedWrap", "");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("NoSignedWrap decoration may not be applied to TypeVoid"));
}

TEST_F(ValidateDecorations, NoSignedWrapOnLabelBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %entry NoSignedWrap", "");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("NoSignedWrap decoration may not be applied to Label"));
}

TEST_F(ValidateDecorations, NoSignedWrapRequiresExtensionBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv);
  EXPECT_NE(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires one of these extensions: "
                        "SPV_KHR_no_integer_wrap_decoration"));
}

TEST_F(ValidateDecorations, NoSignedWrapRequiresExtensionV13Bad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv);
  EXPECT_NE(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires one of these extensions: "
                        "SPV_KHR_no_integer_wrap_decoration"));
}

TEST_F(ValidateDecorations, NoSignedWrapOkInSPV14Good) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapIAddGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpIAdd %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapISubGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpISub %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapIMulGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpIMul %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapShiftLeftLogicalGood) {
  std::string spirv =
      MakeIntegerShader("OpDecorate %val NoSignedWrap",
                        "%val = OpShiftLeftLogical %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapSNegateGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpSNegate %int %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapSRemBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpSRem %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("NoSignedWrap decoration may not be applied to SRem"));
}

TEST_F(ValidateDecorations, NoSignedWrapFAddBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoSignedWrap",
                                        "%val = OpFAdd %float %float0 %float0");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("NoSignedWrap decoration may not be applied to FAdd"));
}

TEST_F(ValidateDecorations, NoSignedWrapExtInstOpenCLGood) {
  std::string spirv =
      MakeIntegerShader("OpDecorate %val NoSignedWrap",
                        "%val = OpExtInst %int %opencl s_abs %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoSignedWrapExtInstGLSLGood) {
  std::string spirv = MakeIntegerShader(
      "OpDecorate %val NoSignedWrap", "%val = OpExtInst %int %glsl SAbs %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

// TODO(dneto): For NoSignedWrap and NoUnsignedWrap, permit
// "OpExtInst for instruction numbers specified in the extended
// instruction-set specifications as accepting this decoration."

// NoUnignedWrap

TEST_F(ValidateDecorations, NoUnsignedWrapOnTypeBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %void NoUnsignedWrap", "");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("NoUnsignedWrap decoration may not be applied to TypeVoid"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapOnLabelBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %entry NoUnsignedWrap", "");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("NoUnsignedWrap decoration may not be applied to Label"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapRequiresExtensionBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv);
  EXPECT_NE(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires one of these extensions: "
                        "SPV_KHR_no_integer_wrap_decoration"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapRequiresExtensionV13Bad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv);
  EXPECT_NE(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires one of these extensions: "
                        "SPV_KHR_no_integer_wrap_decoration"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapOkInSPV14Good) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpIAdd %int %zero %zero", "");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapIAddGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpIAdd %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapISubGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpISub %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapIMulGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpIMul %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapShiftLeftLogicalGood) {
  std::string spirv =
      MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                        "%val = OpShiftLeftLogical %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapSNegateGood) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpSNegate %int %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapSRemBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpSRem %int %zero %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("NoUnsignedWrap decoration may not be applied to SRem"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapFAddBad) {
  std::string spirv = MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                                        "%val = OpFAdd %float %float0 %float0");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("NoUnsignedWrap decoration may not be applied to FAdd"));
}

TEST_F(ValidateDecorations, NoUnsignedWrapExtInstOpenCLGood) {
  std::string spirv =
      MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                        "%val = OpExtInst %int %opencl s_abs %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NoUnsignedWrapExtInstGLSLGood) {
  std::string spirv =
      MakeIntegerShader("OpDecorate %val NoUnsignedWrap",
                        "%val = OpExtInst %int %glsl SAbs %zero");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, AliasedandRestrictBad) {
  const std::string body = R"(
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpSource GLSL 430
OpMemberDecorate %Output 0 Offset 0
OpDecorate %Output BufferBlock
OpDecorate %dataOutput Restrict
OpDecorate %dataOutput Aliased
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%Output = OpTypeStruct %float
%_ptr_Uniform_Output = OpTypePointer Uniform %Output
%dataOutput = OpVariable %_ptr_Uniform_Output Uniform
%main = OpFunction %void None %3
%5 = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(body.c_str());
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("decorated with both Aliased and Restrict is not allowed"));
}

// TODO(dneto): For NoUnsignedWrap and NoUnsignedWrap, permit
// "OpExtInst for instruction numbers specified in the extended
// instruction-set specifications as accepting this decoration."

TEST_F(ValidateDecorations, PSBAliasedRestrictPointerSuccess) {
  const std::string body = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Int64
OpCapability Shader
OpExtension "SPV_EXT_physical_storage_buffer"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint Fragment %main "main"
OpExecutionMode %main OriginUpperLeft
OpDecorate %val1 RestrictPointer
%uint64 = OpTypeInt 64 0
%ptr = OpTypePointer PhysicalStorageBuffer %uint64
%pptr_f = OpTypePointer Function %ptr
%void = OpTypeVoid
%voidfn = OpTypeFunction %void
%main = OpFunction %void None %voidfn
%entry = OpLabel
%val1 = OpVariable %pptr_f Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(body.c_str());
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, PSBAliasedRestrictPointerBoth) {
  const std::string body = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Int64
OpCapability Shader
OpExtension "SPV_EXT_physical_storage_buffer"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint Fragment %main "main"
OpExecutionMode %main OriginUpperLeft
OpDecorate %val1 RestrictPointer
OpDecorate %val1 AliasedPointer
%uint64 = OpTypeInt 64 0
%ptr = OpTypePointer PhysicalStorageBuffer %uint64
%pptr_f = OpTypePointer Function %ptr
%void = OpTypeVoid
%voidfn = OpTypeFunction %void
%main = OpFunction %void None %voidfn
%entry = OpLabel
%val1 = OpVariable %pptr_f Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(body.c_str());
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("decorated with both AliasedPointer and "
                        "RestrictPointer is not allowed"));
}

TEST_F(ValidateDecorations, PSBAliasedRestrictFunctionParamSuccess) {
  const std::string body = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Int64
OpCapability Shader
OpExtension "SPV_EXT_physical_storage_buffer"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint Fragment %main "main"
OpExecutionMode %main OriginUpperLeft
OpDecorate %fparam Restrict
%uint64 = OpTypeInt 64 0
%ptr = OpTypePointer PhysicalStorageBuffer %uint64
%void = OpTypeVoid
%voidfn = OpTypeFunction %void
%fnptr = OpTypeFunction %void %ptr
%main = OpFunction %void None %voidfn
%entry = OpLabel
OpReturn
OpFunctionEnd
%fn = OpFunction %void None %fnptr
%fparam = OpFunctionParameter %ptr
%lab = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(body.c_str());
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, PSBAliasedRestrictFunctionParamBoth) {
  const std::string body = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Int64
OpCapability Shader
OpExtension "SPV_EXT_physical_storage_buffer"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint Fragment %main "main"
OpExecutionMode %main OriginUpperLeft
OpDecorate %fparam Restrict
OpDecorate %fparam Aliased
%uint64 = OpTypeInt 64 0
%ptr = OpTypePointer PhysicalStorageBuffer %uint64
%void = OpTypeVoid
%voidfn = OpTypeFunction %void
%fnptr = OpTypeFunction %void %ptr
%main = OpFunction %void None %voidfn
%entry = OpLabel
OpReturn
OpFunctionEnd
%fn = OpFunction %void None %fnptr
%fparam = OpFunctionParameter %ptr
%lab = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(body.c_str());
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("decorated with both Aliased and Restrict is not allowed"));
}

TEST_F(ValidateDecorations, PSBFPRoundingModeSuccess) {
  std::string spirv = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Shader
OpCapability Linkage
OpCapability StorageBuffer16BitAccess
OpExtension "SPV_EXT_physical_storage_buffer"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %_ FPRoundingMode RTE
OpDecorate %half_ptr_var AliasedPointer
%half = OpTypeFloat 16
%float = OpTypeFloat 32
%float_1_25 = OpConstant %float 1.25
%half_ptr = OpTypePointer PhysicalStorageBuffer %half
%half_pptr_f = OpTypePointer Function %half_ptr
%void = OpTypeVoid
%func = OpTypeFunction %void
%main = OpFunction %void None %func
%main_entry = OpLabel
%half_ptr_var = OpVariable %half_pptr_f Function
%val1 = OpLoad %half_ptr %half_ptr_var
%_ = OpFConvert %half %float_1_25
OpStore %val1 %_ Aligned 2
OpReturn
OpFunctionEnd
  )";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, InvalidStraddle) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpMemberDecorate %inner_struct 0 Offset 0
OpMemberDecorate %inner_struct 1 Offset 4
OpDecorate %outer_struct Block
OpMemberDecorate %outer_struct 0 Offset 0
OpMemberDecorate %outer_struct 1 Offset 8
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%inner_struct = OpTypeStruct %float %float2
%outer_struct = OpTypeStruct %float2 %inner_struct
%ptr_ssbo_outer = OpTypePointer StorageBuffer %outer_struct
%var = OpVariable %ptr_ssbo_outer StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 2 decorated as Block for variable in "
                        "StorageBuffer storage class must follow relaxed "
                        "storage buffer layout rules: member 1 is an "
                        "improperly straddling vector at offset 12"));
}

TEST_F(ValidateDecorations, DescriptorArray) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpMemberDecorate %struct 1 Offset 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%float2 = OpTypeVector %float 2
%struct = OpTypeStruct %float %float2
%struct_array = OpTypeArray %struct %int_2
%ptr_ssbo_array = OpTypePointer StorageBuffer %struct_array
%var = OpVariable %ptr_ssbo_array StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 2 decorated as Block for variable in "
                        "StorageBuffer storage class must follow standard "
                        "storage buffer layout rules: member 1 at offset 1 is "
                        "not aligned to 8"));
}

TEST_F(ValidateDecorations, DescriptorRuntimeArray) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability RuntimeDescriptorArrayEXT
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_EXT_descriptor_indexing"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpMemberDecorate %struct 1 Offset 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%float2 = OpTypeVector %float 2
%struct = OpTypeStruct %float %float2
%struct_array = OpTypeRuntimeArray %struct
%ptr_ssbo_array = OpTypePointer StorageBuffer %struct_array
%var = OpVariable %ptr_ssbo_array StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 2 decorated as Block for variable in "
                        "StorageBuffer storage class must follow standard "
                        "storage buffer layout rules: member 1 at offset 1 is "
                        "not aligned to 8"));
}

TEST_F(ValidateDecorations, MultiDimensionalArray) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array_4 ArrayStride 4
OpDecorate %array_3 ArrayStride 48
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_3 = OpConstant %int 3
%int_4 = OpConstant %int 4
%array_4 = OpTypeArray %int %int_4
%array_3 = OpTypeArray %array_4 %int_3
%struct = OpTypeStruct %array_3
%ptr_struct = OpTypePointer Uniform %struct
%var = OpVariable %ptr_struct Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 2 decorated as Block for variable in "
                        "Uniform storage class must follow standard uniform "
                        "buffer layout rules: member 0 contains an array with "
                        "stride 4 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, ImproperStraddleInArray) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array ArrayStride 24
OpMemberDecorate %inner 0 Offset 0
OpMemberDecorate %inner 1 Offset 4
OpMemberDecorate %inner 2 Offset 12
OpMemberDecorate %inner 3 Offset 16
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%int2 = OpTypeVector %int 2
%inner = OpTypeStruct %int %int2 %int %int
%array = OpTypeArray %inner %int_2
%struct = OpTypeStruct %array
%ptr_struct = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_struct StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 4 decorated as Block for variable in "
                        "StorageBuffer storage class must follow relaxed "
                        "storage buffer layout rules: member 1 is an "
                        "improperly straddling vector at offset 28"));
}

TEST_F(ValidateDecorations, LargeArray) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array ArrayStride 24
OpMemberDecorate %inner 0 Offset 0
OpMemberDecorate %inner 1 Offset 8
OpMemberDecorate %inner 2 Offset 16
OpMemberDecorate %inner 3 Offset 20
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_2000000 = OpConstant %int 2000000
%int2 = OpTypeVector %int 2
%inner = OpTypeStruct %int %int2 %int %int
%array = OpTypeArray %inner %int_2000000
%struct = OpTypeStruct %array
%ptr_struct = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_struct StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_1));
}

// NonReadable/NonWritable

// Returns a SPIR-V shader module with variables in various storage classes,
// parameterizable by which ID should be decorated as NonReadable or
// NonWritable.
std::string ShaderWithNonReadableNonWritableTarget(
    const std::string& target, bool member_decorate = false,
    const std::string& decoration = "NonWritable") {
  const std::string decoration_inst =
      std::string(member_decorate ? "OpMemberDecorate " : "OpDecorate ") +
      target + (member_decorate ? " 0" : "");

  return std::string(R"(
            OpCapability Shader
            OpCapability RuntimeDescriptorArrayEXT
            OpCapability TensorsARM
            OpExtension "SPV_EXT_descriptor_indexing"
            OpExtension "SPV_KHR_storage_buffer_storage_class"
            OpExtension "SPV_ARM_tensors"
            OpMemoryModel Logical GLSL450
            OpEntryPoint Vertex %main "main"
            OpName %label "label"
            OpName %param_f "param_f"
            OpName %param_p "param_p"
            OpName %_ptr_imstor "_ptr_imstor"
            OpName %_ptr_imsam "_ptr_imsam"
            OpName %var_wg "var_wg"
            OpName %var_imsam "var_imsam"
            OpName %var_priv "var_priv"
            OpName %var_func "var_func"
            OpName %simple_struct "simple_struct"

            OpDecorate %struct_b Block
            OpDecorate %struct_b_rtarr Block
            OpMemberDecorate %struct_b 0 Offset 0
            OpMemberDecorate %struct_b_rtarr 0 Offset 0
            OpDecorate %rtarr ArrayStride 4
)") + decoration_inst +
         " " + decoration +

         R"(
      %void = OpTypeVoid
   %void_fn = OpTypeFunction %void
     %float = OpTypeFloat 32
   %float_0 = OpConstant %float 0
   %int     = OpTypeInt 32 0
   %int_2   = OpConstant %int 2
  %struct_b = OpTypeStruct %float
 %rtarr = OpTypeRuntimeArray %float
%struct_b_rtarr = OpTypeStruct %rtarr
%simple_struct = OpTypeStruct %float
 ; storage image
 %imstor = OpTypeImage %float 2D 0 0 0 2 R32f
 ; sampled image
 %imsam = OpTypeImage %float 2D 0 0 0 1 R32f
 ; tensor
 %tensor = OpTypeTensorARM %float %int_2
%array_imstor = OpTypeArray %imstor %int_2
%rta_imstor = OpTypeRuntimeArray %imstor

%_ptr_Uniform_stb        = OpTypePointer Uniform %struct_b
%_ptr_StorageBuffer_stb  = OpTypePointer StorageBuffer %struct_b
%_ptr_StorageBuffer_stb_rtarr  = OpTypePointer StorageBuffer %struct_b_rtarr
%_ptr_Workgroup          = OpTypePointer Workgroup %float
%_ptr_Private            = OpTypePointer Private %float
%_ptr_Function           = OpTypePointer Function %float
%_ptr_imstor             = OpTypePointer UniformConstant %imstor
%_ptr_imsam              = OpTypePointer UniformConstant %imsam
%_ptr_array_imstor       = OpTypePointer UniformConstant %array_imstor
%_ptr_rta_imstor         = OpTypePointer UniformConstant %rta_imstor
%_ptr_tensor_UniformConstant = OpTypePointer UniformConstant %tensor

%extra_fn = OpTypeFunction %void %float %_ptr_Private %_ptr_imstor

%var_ubo = OpVariable %_ptr_Uniform_stb Uniform
%var_ssbo_sb = OpVariable %_ptr_StorageBuffer_stb StorageBuffer
%var_ssbo_sb_rtarr = OpVariable %_ptr_StorageBuffer_stb_rtarr StorageBuffer
%var_wg = OpVariable %_ptr_Workgroup Workgroup
%var_priv = OpVariable %_ptr_Private Private
%var_imstor = OpVariable %_ptr_imstor UniformConstant
%var_imsam = OpVariable %_ptr_imsam UniformConstant
%var_array_imstor = OpVariable %_ptr_array_imstor UniformConstant
%var_rta_imstor = OpVariable %_ptr_rta_imstor UniformConstant
%var_tensor = OpVariable %_ptr_tensor_UniformConstant UniformConstant

  %helper = OpFunction %void None %extra_fn
 %param_f = OpFunctionParameter %float
 %param_p = OpFunctionParameter %_ptr_Private
 %param_pimstor = OpFunctionParameter %_ptr_imstor
%helper_label = OpLabel
%helper_func_var = OpVariable %_ptr_Function Function
            OpReturn
            OpFunctionEnd

    %main = OpFunction %void None %void_fn
   %label = OpLabel
%var_func = OpVariable %_ptr_Function Function
            OpReturn
            OpFunctionEnd
)";
}

TEST_F(ValidateDecorations, NonWritableLabelTargetBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%label");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be a memory object declaration"));
}

TEST_F(ValidateDecorations, NonWritableTypeTargetBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%void");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be a memory object declaration"));
}

TEST_F(ValidateDecorations, NonWritableValueTargetBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%float_0");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be a memory object declaration"));
}

TEST_F(ValidateDecorations, NonWritableValueParamBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%param_f");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a pointer type"));
}

TEST_F(ValidateDecorations, NonWritablePointerParamButWrongTypeBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%param_p");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Target of NonWritable decoration is invalid: must "
          "point to a storage image, tensor variable in UniformConstant "
          "storage class, uniform block, or storage "
          "buffer\n  %param_p = OpFunctionParameter %_ptr_Private_float"));
}

TEST_F(ValidateDecorations, NonWritablePointerParamStorageImageGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%param_pimstor");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarStorageImageGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_imstor");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarSampledImageBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_imsam");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_imsam"));
}

TEST_F(ValidateDecorations, NonWritableVarUboGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_ubo");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarSsboInUniformGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpDecorate %struct_bb BufferBlock
OpMemberDecorate %struct_bb 0 Offset 0
OpDecorate %var_ssbo_u NonWritable
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%struct_bb = OpTypeStruct %float
%_ptr_Uniform_stbb       = OpTypePointer Uniform %struct_bb
%var_ssbo_u = OpVariable %_ptr_Uniform_stbb Uniform
%main = OpFunction %void None %void_fn
%label = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarSsboInStorageBufferGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_ssbo_sb");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableMemberOfSsboInStorageBufferGood) {
  std::string spirv =
      ShaderWithNonReadableNonWritableTarget("%struct_b_rtarr", true);

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableMemberOfStructGood) {
  std::string spirv =
      ShaderWithNonReadableNonWritableTarget("%simple_struct", true);

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, NonWritableTensorVarUniformConstantGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_tensor");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonReadableTensorVarUniformConstantGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget(
      "%var_tensor", false, "NonReadable");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarWorkgroupBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_wg");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_wg"));
}

TEST_F(ValidateDecorations, NonReadableVarWorkgroupBad) {
  std::string spirv =
      ShaderWithNonReadableNonWritableTarget("%var_wg", false, "NonReadable");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonReadable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_wg"));
}

TEST_F(ValidateDecorations, NonWritableVarWorkgroupV14Bad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_wg");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, storage "
                "buffer, or variable in Private or Function storage class\n  "
                "%var_wg"));
}

TEST_F(ValidateDecorations, NonWritableVarPrivateBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_priv");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_priv"));
}

TEST_F(ValidateDecorations, NonWritableVarPrivateV13Bad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_priv");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_priv"));
}

TEST_F(ValidateDecorations, NonWritableVarPrivateV14Good) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_priv");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarPrivateV13TargetV14Bad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_priv");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_priv"));
}

TEST_F(ValidateDecorations, NonWritableVarFunctionBad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_func");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_func"));
}

TEST_F(ValidateDecorations, NonWritableArrayGood) {
  std::string spirv =
      ShaderWithNonReadableNonWritableTarget("%var_array_imstor");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, NonWritableRuntimeArrayGood) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_rta_imstor");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_P(ValidateVulkanCombineDecorationResult, Decorate) {
  const char* const decoration = std::get<0>(GetParam());
  const char* const vuid = std::get<1>(GetParam());
  const TestResult& test_result = std::get<2>(GetParam());

  CodeGenerator generator = CodeGenerator::GetDefaultShaderCodeGenerator();
  generator.before_types_ = "OpDecorate %u32 ";
  generator.before_types_ += decoration;
  generator.before_types_ += "\n";

  EntryPoint entry_point;
  entry_point.name = "main";
  entry_point.execution_model = "Vertex";
  generator.entry_points_.push_back(std::move(entry_point));

  CompileSuccessfully(generator.Build(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(test_result.validation_result,
            ValidateInstructions(SPV_ENV_VULKAN_1_0));
  if (!test_result.error_str.empty()) {
    EXPECT_THAT(getDiagnosticString(), HasSubstr(test_result.error_str));
  }
  if (vuid) {
    EXPECT_THAT(getDiagnosticString(), AnyVUID(vuid));
  }
}

INSTANTIATE_TEST_SUITE_P(
    DecorationAllowListFailure, ValidateVulkanCombineDecorationResult,
    Combine(Values("GLSLShared", "GLSLPacked"),
            Values("VUID-StandaloneSpirv-GLSLShared-04669"),
            Values(TestResult(
                SPV_ERROR_INVALID_ID,
                "is not valid for the Vulkan execution environment."))));

TEST_F(ValidateDecorations, NonWritableVarFunctionV13Bad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_func");

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_func"));
}

TEST_F(ValidateDecorations, NonWritableVarFunctionV14Good) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_func");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, NonWritableVarFunctionV13TargetV14Bad) {
  std::string spirv = ShaderWithNonReadableNonWritableTarget("%var_func");

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Target of NonWritable decoration is invalid: must "
                "point to a storage image, tensor variable in UniformConstant "
                "storage class, uniform block, or storage "
                "buffer\n  %var_func"));
}

TEST_F(ValidateDecorations, BufferBlockV13ValV14Good) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %1 BufferBlock
%1 = OpTypeStruct
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, BufferBlockV14Bad) {
  std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %1 BufferBlock
%1 = OpTypeStruct
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_WRONG_VERSION,
            ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("2nd operand of Decorate: operand BufferBlock(3) "
                        "requires SPIR-V version 1.3 or earlier"));
}

// Component

TEST_F(ValidateDecorations, ComponentDecorationBadTarget) {
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpDecorate %t Component 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%t = OpTypeVector %float 2
%main = OpFunction %void None %3
%5 = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("must be a memory object declaration"));
}

TEST_F(ValidateDecorations, ComponentDecorationBadStorageClass) {
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpDecorate %v Component 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%t = OpTypeVector %float 2
%ptr_private = OpTypePointer Private %t
%v = OpVariable %ptr_private Private
%main = OpFunction %void None %3
%5 = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Target of Component decoration is invalid: must "
                        "point to a Storage Class of Input(1) or Output(3)"));
}

TEST_F(ValidateDecorations, ComponentDecorationBadTypeVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
OpCapability Shader
OpCapability Matrix
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main"
OpDecorate %v Component 0
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%vtype = OpTypeVector %float 4
%t = OpTypeMatrix %vtype 4
%ptr_input = OpTypePointer Input %t
%v = OpVariable %ptr_input Input
%main = OpFunction %void None %3
%5 = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-10583"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Component decoration specified for type"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("is not a scalar or vector"));
}

std::string ShaderWithComponentDecoration(const std::string& type,
                                          const std::string& decoration) {
  return R"(
OpCapability Shader
OpCapability Int64
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %entryPointOutput
OpExecutionMode %main OriginUpperLeft
OpDecorate %entryPointOutput Location 0
OpDecorate %entryPointOutput )" +
         decoration + R"(
%void = OpTypeVoid
%3 = OpTypeFunction %void
%float = OpTypeFloat 32
%v3float = OpTypeVector %float 3
%v4float = OpTypeVector %float 4
%uint = OpTypeInt 32 0
%uint64 = OpTypeInt 64 0
%v2uint64 = OpTypeVector %uint64 2
%v3uint64 = OpTypeVector %uint64 3
%uint_2 = OpConstant %uint 2
%arr_v3float_uint_2 = OpTypeArray %v3float %uint_2
%float_0 = OpConstant %float 0
%_ptr_Output_type = OpTypePointer Output %)" + type + R"(
%entryPointOutput = OpVariable %_ptr_Output_type Output
%main = OpFunction %void None %3
%5 = OpLabel
OpReturn
OpFunctionEnd
)";
}

TEST_F(ValidateDecorations, ComponentDecorationIntGood0Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint", "Component 0");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationIntGood1Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationIntGood2Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint", "Component 2");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationIntGood3Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint", "Component 3");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationIntBad4Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint", "Component 4");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04920"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Component decoration value must not be greater than 3"));
}

TEST_F(ValidateDecorations, ComponentDecorationVector3GoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v3float", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationVector4GoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v4float", "Component 0");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationVector4Bad1Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v4float", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04921"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Sequence of components starting with 1 "
                        "and ending with 4 gets larger than 3"));
}

TEST_F(ValidateDecorations, ComponentDecorationVector4Bad3Vulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v4float", "Component 3");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04921"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Sequence of components starting with 3 "
                        "and ending with 6 gets larger than 3"));
}

TEST_F(ValidateDecorations, ComponentDecorationArrayGoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv =
      ShaderWithComponentDecoration("arr_v3float_uint_2", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationArrayBadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv =
      ShaderWithComponentDecoration("arr_v3float_uint_2", "Component 2");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04921"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Sequence of components starting with 2 "
                        "and ending with 4 gets larger than 3"));
}

TEST_F(ValidateDecorations, ComponentDecoration64ScalarGoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint64", "Component 0");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, ComponentDecoration64Scalar1BadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint64", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04923"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Component decoration value must not be 1 or 3 for "
                        "64-bit data types"));
}

TEST_F(ValidateDecorations, ComponentDecoration64Scalar2GoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint64", "Component 2");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, ComponentDecoration64Scalar3BadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("uint64", "Component 3");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04923"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Component decoration value must not be 1 or 3 for "
                        "64-bit data types"));
}

TEST_F(ValidateDecorations, ComponentDecoration64Vec0GoodVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v2uint64", "Component 0");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, ComponentDecoration64Vec1BadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v2uint64", "Component 1");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04923"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Component decoration value must not be 1 or 3 for "
                        "64-bit data types"));
}

TEST_F(ValidateDecorations, ComponentDecoration64Vec2BadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v2uint64", "Component 2");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04922"));
  HasSubstr(
      "Sequence of components starting with 2 "
      "and ending with 6 gets larger than 3");
}

TEST_F(ValidateDecorations, ComponentDecoration64VecWideBadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = ShaderWithComponentDecoration("v3uint64", "Component 0");

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-07703"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Component decoration only allowed on 64-bit scalar "
                        "and 2-component vector"));
}

TEST_F(ValidateDecorations, ComponentDecorationBlockGood) {
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %4 "main" %9 %12
OpExecutionMode %4 OriginUpperLeft
OpDecorate %9 Location 0
OpMemberDecorate %block 0 Location 2
OpMemberDecorate %block 0 Component 1
OpDecorate %block Block
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%float = OpTypeFloat 32
%vec3 = OpTypeVector %float 3
%8 = OpTypePointer Output %vec3
%9 = OpVariable %8 Output
%block = OpTypeStruct %vec3
%11 = OpTypePointer Input %block
%12 = OpVariable %11 Input
%int = OpTypeInt 32 1
%14 = OpConstant %int 0
%15 = OpTypePointer Input %vec3
%4 = OpFunction %2 None %3
%5 = OpLabel
%16 = OpAccessChain %15 %12 %14
%17 = OpLoad %vec3 %16
OpStore %9 %17
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(), Eq(""));
}

TEST_F(ValidateDecorations, ComponentDecorationBlockBadVulkan) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %4 "main" %9 %12
OpExecutionMode %4 OriginUpperLeft
OpDecorate %9 Location 0
OpMemberDecorate %block 0 Location 2
OpMemberDecorate %block 0 Component 2
OpDecorate %block Block
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%float = OpTypeFloat 32
%vec3 = OpTypeVector %float 3
%8 = OpTypePointer Output %vec3
%9 = OpVariable %8 Output
%block = OpTypeStruct %vec3
%11 = OpTypePointer Input %block
%12 = OpVariable %11 Input
%int = OpTypeInt 32 1
%14 = OpConstant %int 0
%15 = OpTypePointer Input %vec3
%4 = OpFunction %2 None %3
%5 = OpLabel
%16 = OpAccessChain %15 %12 %14
%17 = OpLoad %vec3 %16
OpStore %9 %17
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Component-04921"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Sequence of components starting with 2 "
                        "and ending with 4 gets larger than 3"));
}

TEST_F(ValidateDecorations, ComponentDecorationFunctionParameter) {
  std::string spirv = R"(
              OpCapability Shader
              OpMemoryModel Logical GLSL450
              OpEntryPoint Vertex %main "main"

              OpDecorate %param_f Component 0

      %void = OpTypeVoid
   %void_fn = OpTypeFunction %void
     %float = OpTypeFloat 32
   %float_0 = OpConstant %float 0
   %int     = OpTypeInt 32 0
   %int_2   = OpConstant %int 2
  %struct_b = OpTypeStruct %float

%extra_fn = OpTypeFunction %void %float

  %helper = OpFunction %void None %extra_fn
 %param_f = OpFunctionParameter %float
%helper_label = OpLabel
            OpReturn
            OpFunctionEnd

    %main = OpFunction %void None %void_fn
   %label = OpLabel
            OpReturn
            OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a pointer type"));
}

TEST_F(ValidateDecorations, VulkanStorageBufferBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%ptr_ssbo = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_ssbo StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanStorageBufferMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%ptr_ssbo = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_ssbo StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PushConstant-06675"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables "
                        "must be identified with a Block decoration"));
}

TEST_F(ValidateDecorations, VulkanStorageBufferArrayMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%uint_4 = OpConstant %uint 4
%struct = OpTypeStruct %uint
%array = OpTypeArray %struct %uint_4
%ptr_ssbo = OpTypePointer StorageBuffer %array
%var = OpVariable %ptr_ssbo StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PushConstant-06675"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables "
                        "must be identified with a Block decoration"));
}

TEST_F(ValidateDecorations, VulkanStorageBufferRuntimeArrayMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability RuntimeDescriptorArrayEXT
OpExtension "SPV_EXT_descriptor_indexing"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%array = OpTypeRuntimeArray %struct
%ptr_ssbo = OpTypePointer StorageBuffer %array
%var = OpVariable %ptr_ssbo StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PushConstant-06675"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables "
                        "must be identified with a Block decoration"));
}

TEST_F(ValidateDecorations, VulkanUniformBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%ptr_ubo = OpTypePointer Uniform %struct
%var = OpVariable %ptr_ubo Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanUniformBufferBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct BufferBlock
OpMemberDecorate %struct 0 Offset 0
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%ptr_ubo = OpTypePointer Uniform %struct
%var = OpVariable %ptr_ubo Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanUniformMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%ptr_ubo = OpTypePointer Uniform %struct
%var = OpVariable %ptr_ubo Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Uniform-06676"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables must be "
                        "identified with a Block or BufferBlock decoration"));
}

TEST_F(ValidateDecorations, VulkanUniformArrayMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%uint_4 = OpConstant %uint 4
%struct = OpTypeStruct %uint
%array = OpTypeArray %struct %uint_4
%ptr_ubo = OpTypePointer Uniform %array
%var = OpVariable %ptr_ubo Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Uniform-06676"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables must be "
                        "identified with a Block or BufferBlock decoration"));
}

TEST_F(ValidateDecorations, VulkanUniformRuntimeArrayMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability RuntimeDescriptorArrayEXT
OpExtension "SPV_EXT_descriptor_indexing"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%struct = OpTypeStruct %uint
%array = OpTypeRuntimeArray %struct
%ptr_ubo = OpTypePointer Uniform %array
%var = OpVariable %ptr_ubo Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Uniform-06676"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("From Vulkan spec:\nSuch variables must be "
                        "identified with a Block or BufferBlock decoration"));
}

TEST_F(ValidateDecorations, VulkanArrayStrideZero) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array ArrayStride 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%array = OpTypeArray %int %int_4
%struct = OpTypeStruct %array
%ptr_ssbo_struct = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_ssbo_struct StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("contains an array with stride 0"));
}

TEST_F(ValidateDecorations, VulkanArrayStrideTooSmall) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %inner ArrayStride 4
OpDecorate %outer ArrayStride 4
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%inner = OpTypeArray %int %int_4
%outer = OpTypeArray %inner %int_4
%struct = OpTypeStruct %outer
%ptr_ssbo_struct = OpTypePointer StorageBuffer %struct
%var = OpVariable %ptr_ssbo_struct StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "contains an array with stride 4, but with an element size of 16"));
}

TEST_F(ValidateDecorations, FunctionsWithOpGroupDecorate) {
  std::string spirv = R"(
                OpCapability Addresses
                OpCapability Linkage
                OpCapability Kernel
                OpCapability Int8
           %1 = OpExtInstImport "OpenCL.std"
                OpMemoryModel Physical32 OpenCL
                OpName %foo "foo"
                OpName %entry "entry"
                OpName %bar "bar"
                OpName %entry_0 "entry"
                OpName %k "k"
                OpName %entry_1 "entry"
                OpName %b "b"
                OpDecorate %28 FuncParamAttr Zext
          %28 = OpDecorationGroup
                OpDecorate %k LinkageAttributes "k" Export
                OpDecorate %foo LinkageAttributes "foo" Export
                OpDecorate %bar LinkageAttributes "bar" Export
                OpDecorate %b Alignment 1
                OpGroupDecorate %28 %foo %bar
       %uchar = OpTypeInt 8 0
        %bool = OpTypeBool
           %3 = OpTypeFunction %bool
        %void = OpTypeVoid
          %10 = OpTypeFunction %void
 %_ptr_Function_uchar = OpTypePointer Function %uchar
        %true = OpConstantTrue %bool
         %foo = OpFunction %bool DontInline %3
       %entry = OpLabel
                OpReturnValue %true
                OpFunctionEnd
         %bar = OpFunction %bool DontInline %3
     %entry_0 = OpLabel
                OpReturnValue %true
                OpFunctionEnd
           %k = OpFunction %void DontInline %10
     %entry_1 = OpLabel
           %b = OpVariable %_ptr_Function_uchar Function
                OpReturn
                OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidateDecorations, LocationVariableGood) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %in_var Location 0
%float = OpTypeFloat 32
%ptr_input_float = OpTypePointer Input %float
%in_var = OpVariable %ptr_input_float Input
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, LocationStructMemberGood) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpMemberDecorate %struct 0 Location 0
%float = OpTypeFloat 32
%struct = OpTypeStruct %float
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateDecorations, LocationStructBad) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %struct Location 0
%float = OpTypeFloat 32
%struct = OpTypeStruct %float
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a variable"));
}

TEST_F(ValidateDecorations, LocationFloatBad) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %float Location 0
%float = OpTypeFloat 32
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a variable"));
}

TEST_F(ValidateDecorations, WorkgroupSingleBlockVariable) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupBlockVariableRequiresV14) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_ERROR_WRONG_VERSION,
            ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires SPIR-V version 1.4 or later"));
}

TEST_F(ValidateDecorations, WorkgroupSingleNonBlockVariable) {
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %a
               OpExecutionMode %main LocalSize 8 1 1
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
          %a = OpVariable %_ptr_Workgroup_int Workgroup
      %int_2 = OpConstant %int 2
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpStore %a %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupMultiBlockVariable) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_ %__0
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
               OpMemberDecorate %second 0 Offset 0
               OpDecorate %second Block
               OpDecorate %_ Aliased
               OpDecorate %__0 Aliased
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
     %second = OpTypeStruct %int
%_ptr_Workgroup_second = OpTypePointer Workgroup %second
        %__0 = OpVariable %_ptr_Workgroup_second Workgroup
      %int_3 = OpConstant %int 3
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
         %18 = OpAccessChain %_ptr_Workgroup_int %__0 %int_0
               OpStore %18 %int_3
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupBlockVariableWith8BitType) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Int8
               OpCapability WorkgroupMemoryExplicitLayout8BitAccessKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 2 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %char = OpTypeInt 8 1
      %first = OpTypeStruct %char
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
     %char_2 = OpConstant %char 2
%_ptr_Workgroup_char = OpTypePointer Workgroup %char
       %main = OpFunction %void None %3
          %5 = OpLabel
         %14 = OpAccessChain %_ptr_Workgroup_char %_ %int_0
               OpStore %14 %char_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupMultiNonBlockVariable) {
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %a %b
               OpExecutionMode %main LocalSize 8 1 1
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
          %a = OpVariable %_ptr_Workgroup_int Workgroup
      %int_2 = OpConstant %int 2
          %b = OpVariable %_ptr_Workgroup_int Workgroup
      %int_3 = OpConstant %int 3
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpStore %a %int_2
               OpStore %b %int_3
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupBlockVariableWith16BitType) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Float16
               OpCapability Int16
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpCapability WorkgroupMemoryExplicitLayout16BitAccessKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 2 1 1
               OpMemberDecorate %first 0 Offset 0
               OpMemberDecorate %first 1 Offset 2
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %short = OpTypeInt 16 1
       %half = OpTypeFloat 16
      %first = OpTypeStruct %short %half
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %short_3 = OpConstant %short 3
%_ptr_Workgroup_short = OpTypePointer Workgroup %short
      %int_1 = OpConstant %int 1
%half_0x1_898p_3 = OpConstant %half 0x1.898p+3
%_ptr_Workgroup_half = OpTypePointer Workgroup %half
       %main = OpFunction %void None %3
          %5 = OpLabel
         %15 = OpAccessChain %_ptr_Workgroup_short %_ %int_0
               OpStore %15 %short_3
         %19 = OpAccessChain %_ptr_Workgroup_half %_ %int_1
               OpStore %19 %half_0x1_898p_3
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS,
	    ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
}

TEST_F(ValidateDecorations, WorkgroupBlockVariableScalarLayout) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %B
               OpSource GLSL 450
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 4
               OpMemberDecorate %S 2 Offset 16
               OpMemberDecorate %S 3 Offset 28
               OpDecorate %S Block
               OpDecorate %B Aliased
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
          %S = OpTypeStruct %float %v3float %v3float %v3float
%_ptr_Workgroup_S = OpTypePointer Workgroup %S
          %B = OpVariable %_ptr_Workgroup_S Workgroup
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  spvValidatorOptionsSetWorkgroupScalarBlockLayout(getValidatorOptions(), true);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4))
      << getDiagnosticString();
}

TEST_F(ValidateDecorations, WorkgroupMixBlockAndNonBlockBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_ %b
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
               OpDecorate %_ Aliased
               OpDecorate %b Aliased
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
          %b = OpVariable %_ptr_Workgroup_int Workgroup
      %int_3 = OpConstant %int 3
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpStore %b %int_3
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("either all or none of the Workgroup Storage Class variables "
                "in the entry point interface must point to struct types "
                "decorated with Block"));
}

TEST_F(ValidateDecorations, WorkgroupMultiBlockVariableMissingAliased) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_ %__0
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 0
               OpDecorate %first Block
               OpMemberDecorate %second 0 Offset 0
               OpDecorate %second Block
               OpDecorate %_ Aliased
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
     %second = OpTypeStruct %int
%_ptr_Workgroup_second = OpTypePointer Workgroup %second
        %__0 = OpVariable %_ptr_Workgroup_second Workgroup
      %int_3 = OpConstant %int 3
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
         %18 = OpAccessChain %_ptr_Workgroup_int %__0 %int_0
               OpStore %18 %int_3
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("more than one Workgroup Storage Class variable in the "
                "entry point interface point to a type decorated with Block, "
                "all of them must be decorated with Aliased"));
}

TEST_F(ValidateDecorations, WorkgroupSingleBlockVariableNotAStruct) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 8 1 1
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %int_3 = OpConstant %int 3
      %first = OpTypeArray %int %int_3
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("must be a structure type"));
}

TEST_F(ValidateDecorations, WorkgroupSingleBlockVariableMissingLayout) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 8 1 1
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1_SPIRV_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Block must be explicitly laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, WorkgroupSingleBlockVariableBadLayout) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability WorkgroupMemoryExplicitLayoutKHR
               OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 8 1 1
               OpMemberDecorate %first 0 Offset 1
               OpDecorate %first Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %first = OpTypeStruct %int
%_ptr_Workgroup_first = OpTypePointer Workgroup %first
          %_ = OpVariable %_ptr_Workgroup_first Workgroup
      %int_0 = OpConstant %int 0
      %int_2 = OpConstant %int 2
%_ptr_Workgroup_int = OpTypePointer Workgroup %int
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_Workgroup_int %_ %int_0
               OpStore %13 %int_2
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1_SPIRV_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Block for variable in Workgroup storage class must follow "
                "relaxed storage buffer layout rules: "
                "member 0 at offset 1 is not aligned to 4"));
}

TEST_F(ValidateDecorations, WorkgroupBlockNoCapability) {
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %struct 0 Offset 0
               OpMemberDecorate %struct 1 Offset 4
               OpDecorate %struct Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
     %struct = OpTypeStruct %int %int
%ptr_workgroup = OpTypePointer Workgroup %struct
          %_ = OpVariable %ptr_workgroup Workgroup
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1_SPIRV_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Workgroup Storage Class variables can't be decorated with Block "
          "unless declaring the WorkgroupMemoryExplicitLayoutKHR capability"));
}

TEST_F(ValidateDecorations, BadMatrixStrideUniform) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 3
OpMemberDecorate %block 0 ColMajor
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%float4 = OpTypeVector %float 4
%matrix4x4 = OpTypeMatrix %float4 4
%block = OpTypeStruct %matrix4x4
%block_ptr = OpTypePointer Uniform %block
%var = OpVariable %block_ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in Uniform storage "
          "class must follow standard uniform buffer layout rules: member 0 is "
          "a matrix with stride 3 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, BadMatrixStrideStorageBuffer) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 3
OpMemberDecorate %block 0 ColMajor
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%float4 = OpTypeVector %float 4
%matrix4x4 = OpTypeMatrix %float4 4
%block = OpTypeStruct %matrix4x4
%block_ptr = OpTypePointer StorageBuffer %block
%var = OpVariable %block_ptr StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in StorageBuffer "
          "storage class must follow standard storage buffer layout rules: "
          "member 0 is a matrix with stride 3 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, BadMatrixStridePushConstant) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 3
OpMemberDecorate %block 0 ColMajor
%void = OpTypeVoid
%float = OpTypeFloat 32
%float4 = OpTypeVector %float 4
%matrix4x4 = OpTypeMatrix %float4 4
%block = OpTypeStruct %matrix4x4
%block_ptr = OpTypePointer PushConstant %block
%var = OpVariable %block_ptr PushConstant
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in PushConstant "
          "storage class must follow standard storage buffer layout rules: "
          "member 0 is a matrix with stride 3 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, BadMatrixStrideStorageBufferScalarLayout) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 3
OpMemberDecorate %block 0 RowMajor
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%float4 = OpTypeVector %float 4
%matrix4x4 = OpTypeMatrix %float4 4
%block = OpTypeStruct %matrix4x4
%block_ptr = OpTypePointer StorageBuffer %block
%var = OpVariable %block_ptr StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  options_->scalar_block_layout = true;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Structure id 2 decorated as Block for variable in StorageBuffer "
          "storage class must follow scalar storage buffer layout rules: "
          "member 0 is a matrix with stride 3 not satisfying alignment to 4"));
}

TEST_F(ValidateDecorations, MissingOffsetStructNestedInArray) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %array ArrayStride 4
OpDecorate %outer Block
OpMemberDecorate %outer 0 Offset 0
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%inner = OpTypeStruct %int
%array = OpTypeArray %inner %int_4
%outer = OpTypeStruct %array
%ptr_ssbo_outer = OpTypePointer StorageBuffer %outer
%var = OpVariable %ptr_ssbo_outer StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Structure id 3 decorated as Block must be explicitly "
                        "laid out with Offset decorations"));
}

TEST_F(ValidateDecorations, AllOnesOffset) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %outer Block
OpMemberDecorate %outer 0 Offset 0
OpMemberDecorate %struct 0 Offset 4294967295
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%outer = OpTypeStruct %struct
%ptr = OpTypePointer Uniform %outer
%var = OpVariable %ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("decorated as Block must be explicitly laid out with "
                        "Offset decorations"));
}

TEST_F(ValidateDecorations, PerVertexVulkanGood) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability FragmentBarycentricKHR
               OpExtension "SPV_KHR_fragment_shader_barycentric"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %vertexIDs
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %vertexIDs Location 0
               OpDecorate %vertexIDs PerVertexKHR
       %void = OpTypeVoid
       %func = OpTypeFunction %void
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
%ptrFloat = OpTypePointer Input %float
     %uint_3 = OpConstant %uint 3
%floatArray = OpTypeArray %float %uint_3
%ptrFloatArray = OpTypePointer Input %floatArray
  %vertexIDs = OpVariable %ptrFloatArray Input
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
       %main = OpFunction %void None %func
      %label = OpLabel
     %access = OpAccessChain %ptrFloat %vertexIDs %int_0
       %load = OpLoad %float %access
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, PerVertexVulkanOutput) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability FragmentBarycentricKHR
               OpExtension "SPV_KHR_fragment_shader_barycentric"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %vertexIDs
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %vertexIDs Location 0
               OpDecorate %vertexIDs PerVertexKHR
       %void = OpTypeVoid
       %func = OpTypeFunction %void
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
%ptrFloat = OpTypePointer Output %float
     %uint_3 = OpConstant %uint 3
%floatArray = OpTypeArray %float %uint_3
%ptrFloatArray = OpTypePointer Output %floatArray
  %vertexIDs = OpVariable %ptrFloatArray Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
       %main = OpFunction %void None %func
      %label = OpLabel
     %access = OpAccessChain %ptrFloat %vertexIDs %int_0
       %load = OpLoad %float %access
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PerVertexKHR-06777"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("storage class must be Input"));
}

TEST_F(ValidateDecorations, PerVertexVulkanNonFragment) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability FragmentBarycentricKHR
               OpExtension "SPV_KHR_fragment_shader_barycentric"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %vertexIDs
               OpDecorate %vertexIDs Location 0
               OpDecorate %vertexIDs PerVertexKHR
       %void = OpTypeVoid
       %func = OpTypeFunction %void
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
%ptrFloat = OpTypePointer Input %float
     %uint_3 = OpConstant %uint 3
%floatArray = OpTypeArray %float %uint_3
%ptrFloatArray = OpTypePointer Input %floatArray
  %vertexIDs = OpVariable %ptrFloatArray Input
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
       %main = OpFunction %void None %func
      %label = OpLabel
     %access = OpAccessChain %ptrFloat %vertexIDs %int_0
       %load = OpLoad %float %access
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-PerVertexKHR-06777"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "PerVertexKHR can only be applied to Fragment Execution Models"));
}

TEST_F(ValidateDecorations, PerVertexVulkanNonArray) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability FragmentBarycentricKHR
               OpExtension "SPV_KHR_fragment_shader_barycentric"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %vertexIDs
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %vertexIDs Location 0
               OpDecorate %vertexIDs PerVertexKHR
       %void = OpTypeVoid
       %func = OpTypeFunction %void
      %float = OpTypeFloat 32
   %ptrFloat = OpTypePointer Input %float
  %vertexIDs = OpVariable %ptrFloat Input
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
       %main = OpFunction %void None %func
      %label = OpLabel
       %load = OpLoad %float %vertexIDs
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_DATA, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Input-06778"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("PerVertexKHR must be declared as arrays"));
}

TEST_F(ValidateDecorations, RelaxedPrecisionDecorationOnNumericTypeBad) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %float RelaxedPrecision
       %void = OpTypeVoid
      %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
       %main = OpFunction %void None %voidfn
      %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateAndRetrieveValidationState(env));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("RelaxPrecision decoration cannot be applied to a type"));
}

TEST_F(ValidateDecorations, RelaxedPrecisionDecorationOnStructMember) {
  const spv_target_env env = SPV_ENV_VULKAN_1_0;
  std::string spirv = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpMemberDecorate %struct 0 RelaxedPrecision
       %void = OpTypeVoid
     %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
     %struct = OpTypeStruct %float
       %main = OpFunction %void None %voidfn
      %label = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, env);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState(env));
}

TEST_F(ValidateDecorations, VulkanFlatMultipleInterfaceGood) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Geometry
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %layer %gl_Layer
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %layer Location 0
               OpDecorate %gl_Layer Flat
               OpDecorate %gl_Layer BuiltIn Layer
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Output_int = OpTypePointer Output %int
      %layer = OpVariable %_ptr_Output_int Output
%_ptr_Input_int = OpTypePointer Input %int
   %gl_Layer = OpVariable %_ptr_Input_int Input
       %main = OpFunction %void None %3
          %5 = OpLabel
         %11 = OpLoad %int %gl_Layer
               OpStore %layer %11
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanFlatMultipleInterfaceBad) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Geometry
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %layer %gl_Layer
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %layer Location 0
               OpDecorate %gl_Layer BuiltIn Layer
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Output_int = OpTypePointer Output %int
      %layer = OpVariable %_ptr_Output_int Output
%_ptr_Input_int = OpTypePointer Input %int
   %gl_Layer = OpVariable %_ptr_Input_int Input
       %main = OpFunction %void None %3
          %5 = OpLabel
         %11 = OpLoad %int %gl_Layer
               OpStore %layer %11
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Flat-04744"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Fragment OpEntryPoint operand 4 with Input interfaces with integer "
          "or float type must have a Flat decoration for Entry Point id 2."));
}

TEST_F(ValidateDecorations, VulkanNoFlatFloat32) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %in Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
         %in = OpVariable %_ptr_Input_float Input
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_float Function
         %11 = OpLoad %float %in
               OpStore %b %11
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanNoFlatFloat64) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Float64
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %in Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
     %double = OpTypeFloat 64
%_ptr_Function_double = OpTypePointer Function %double
%_ptr_Input_double = OpTypePointer Input %double
         %in = OpVariable %_ptr_Input_double Input
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_double Function
         %11 = OpLoad %double %in
               OpStore %b %11
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Flat-04744"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Fragment OpEntryPoint operand 3 with Input interfaces with integer "
          "or float type must have a Flat decoration for Entry Point id 2."));
}

TEST_F(ValidateDecorations, VulkanNoFlatVectorFloat64) {
  std::string spirv = R"(
               OpCapability Shader
               OpCapability Float64
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %in Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
     %double = OpTypeFloat 64
   %v2double = OpTypeVector %double 2
%_ptr_Function_v2double = OpTypePointer Function %v2double
%_ptr_Input_v2double = OpTypePointer Input %v2double
         %in = OpVariable %_ptr_Input_v2double Input
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_v2double Function
         %11 = OpLoad %v2double %in
               OpStore %b %11
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, VulkanNoFlatIntVector) {
  std::string spirv = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %in Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
      %v2int = OpTypeVector %int 2
%_ptr_Function_v2int = OpTypePointer Function %v2int
%_ptr_Input_v2int = OpTypePointer Input %v2int
         %in = OpVariable %_ptr_Input_v2int Input
       %main = OpFunction %void None %3
          %5 = OpLabel
          %b = OpVariable %_ptr_Function_v2int Function
         %12 = OpLoad %v2int %in
               OpStore %b %12
               OpReturn
               OpFunctionEnd
  )";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Flat-04744"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Fragment OpEntryPoint operand 3 with Input interfaces with integer "
          "or float type must have a Flat decoration for Entry Point id 2."));
}

TEST_P(ValidateDecorationString, VulkanOutputInvalidInterface) {
  const std::string decoration = GetParam();
  std::stringstream ss;
  ss << R"(
               OpCapability Shader
               OpCapability SampleRateShading
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %out
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpDecorate %out )"
     << decoration << R"(
               OpDecorate %out Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Output_int = OpTypePointer Output %int
        %out = OpVariable %_ptr_Output_int Output
      %int_1 = OpConstant %int 1
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpStore %out %int_1
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(ss.str(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Flat-06201"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("decorated variable must not be used in fragment execution "
                "model as an Output storage class for Entry Point id 2."));
}

TEST_P(ValidateDecorationString, VulkanVertexInputInvalidInterface) {
  const std::string decoration = GetParam();
  std::stringstream ss;
  ss << R"(
               OpCapability Shader
               OpCapability SampleRateShading
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %out %in
               OpSource GLSL 450
               OpDecorate %in )"
     << decoration << R"(
               OpDecorate %out Location 0
               OpDecorate %in Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Output_int = OpTypePointer Output %int
          %out = OpVariable %_ptr_Output_int Output
%_ptr_Input_int = OpTypePointer Input %int
          %in = OpVariable %_ptr_Input_int Input
       %main = OpFunction %void None %3
          %5 = OpLabel
         %11 = OpLoad %int %in
               OpStore %out %11
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(ss.str(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-Flat-06202"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("decorated variable must not be used in vertex execution model "
                "as an Input storage class for Entry Point id 2."));
}

INSTANTIATE_TEST_SUITE_P(FragmentInputInterface, ValidateDecorationString,
                         ::testing::Values("Flat", "NoPerspective", "Sample",
                                           "Centroid"));

TEST_F(ValidateDecorations, NVBindlessSamplerArrayInBlock) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability BindlessTextureNV
               OpExtension "SPV_NV_bindless_texture"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpSamplerImageAddressingModeNV 64
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %UBO "UBO"
               OpMemberName %UBO 0 "uboSampler"
               OpName %_ ""
               OpDecorate %array ArrayStride 16
               OpMemberDecorate %UBO 0 Offset 0
               OpDecorate %UBO Block
               OpDecorate %_ DescriptorSet 0
               OpDecorate %_ Binding 2
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
          %7 = OpTypeImage %float 2D 0 0 0 1 Unknown
          %8 = OpTypeSampledImage %7
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
      %array = OpTypeArray %8 %uint_3
        %UBO = OpTypeStruct %array
    %pointer = OpTypePointer Uniform %UBO
          %_ = OpVariable %pointer Uniform
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_UNIVERSAL_1_3));
}

TEST_F(ValidateDecorations, Std140ColMajorMat2x2) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 0 MatrixStride 8
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer Uniform %block
%var = OpVariable %ptr_block Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "member 0 is a matrix with stride 8 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, Std140RowMajorMat2x2) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 RowMajor
OpMemberDecorate %block 0 MatrixStride 8
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer Uniform %block
%var = OpVariable %ptr_block Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "member 0 is a matrix with stride 8 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, Std140ColMajorMat4x2) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 0 MatrixStride 8
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 4
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer Uniform %block
%var = OpVariable %ptr_block Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "member 0 is a matrix with stride 8 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, Std140ColMajorMat2x3) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 0 MatrixStride 12
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float3 = OpTypeVector %float 3
%matrix = OpTypeMatrix %float3 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer Uniform %block
%var = OpVariable %ptr_block Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("member 0 is a matrix with stride 12 not satisfying "
                        "alignment to 16"));
}

TEST_F(ValidateDecorations, MatrixMissingMajornessUniform) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer Uniform %block
%var = OpVariable %ptr_block Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "must be explicitly laid out with RowMajor or ColMajor decorations"));
}

TEST_F(ValidateDecorations, MatrixMissingMajornessStorageBuffer) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer StorageBuffer %block
%var = OpVariable %ptr_block StorageBuffer
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "must be explicitly laid out with RowMajor or ColMajor decorations"));
}

TEST_F(ValidateDecorations, MatrixMissingMajornessPushConstant) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix
%ptr_block = OpTypePointer PushConstant %block
%var = OpVariable %ptr_block PushConstant
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "must be explicitly laid out with RowMajor or ColMajor decorations"));
}

TEST_F(ValidateDecorations, StructWithRowAndColMajor) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 1 Offset 32
OpMemberDecorate %block 1 MatrixStride 16
OpMemberDecorate %block 1 RowMajor
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%float = OpTypeFloat 32
%float2 = OpTypeVector %float 2
%matrix = OpTypeMatrix %float2 2
%block = OpTypeStruct %matrix %matrix
%ptr_block = OpTypePointer PushConstant %block
%var = OpVariable %ptr_block PushConstant
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, PhysicalStorageBufferWithOffset) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Int64
OpCapability PhysicalStorageBufferAddresses
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint GLCompute %main "main" %pc
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %pc_block Block
OpMemberDecorate %pc_block 0 Offset 0
OpMemberDecorate %pssbo_struct 0 Offset 0
%void = OpTypeVoid
%long = OpTypeInt 64 0
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_0 = OpConstant %int 0
%pc_block = OpTypeStruct %long
%pc_block_ptr = OpTypePointer PushConstant %pc_block
%pc_long_ptr = OpTypePointer PushConstant %long
%pc = OpVariable %pc_block_ptr PushConstant
%pssbo_struct = OpTypeStruct %float
%pssbo_ptr = OpTypePointer PhysicalStorageBuffer %pssbo_struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%pc_gep = OpAccessChain %pc_long_ptr %pc %int_0
%addr = OpLoad %long %pc_gep
%ptr = OpConvertUToPtr %pssbo_ptr %addr
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateDecorations, UntypedVariableDuplicateInterface) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpExtension "SPV_KHR_untyped_pointers"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %var %var
OpName %var "var"
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR Workgroup
%var = OpUntypedVariableKHR %ptr Workgroup %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Non-unique OpEntryPoint interface '2[%var]' is disallowed"));
}

TEST_F(ValidateDecorations, PhysicalStorageBufferMissingOffset) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Int64
OpCapability PhysicalStorageBufferAddresses
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint GLCompute %main "main" %pc
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %pc_block Block
OpMemberDecorate %pc_block 0 Offset 0
%void = OpTypeVoid
%long = OpTypeInt 64 0
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_0 = OpConstant %int 0
%pc_block = OpTypeStruct %long
%pc_block_ptr = OpTypePointer PushConstant %pc_block
%pc_long_ptr = OpTypePointer PushConstant %long
%pc = OpVariable %pc_block_ptr PushConstant
%pssbo_struct = OpTypeStruct %float
%pssbo_ptr = OpTypePointer PhysicalStorageBuffer %pssbo_struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%pc_gep = OpAccessChain %pc_long_ptr %pc %int_0
%addr = OpLoad %long %pc_gep
%ptr = OpConvertUToPtr %pssbo_ptr %addr
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("decorated as Block for variable in PhysicalStorageBuffer "
                "storage class must follow relaxed storage buffer layout "
                "rules: member 0 is missing an Offset decoration"));
}

TEST_F(ValidateDecorations, PhysicalStorageBufferMissingArrayStride) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Int64
OpCapability PhysicalStorageBufferAddresses
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint GLCompute %main "main" %pc
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %pc_block Block
OpMemberDecorate %pc_block 0 Offset 0
%void = OpTypeVoid
%long = OpTypeInt 64 0
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_0 = OpConstant %int 0
%int_4 = OpConstant %int 4
%pc_block = OpTypeStruct %long
%pc_block_ptr = OpTypePointer PushConstant %pc_block
%pc_long_ptr = OpTypePointer PushConstant %long
%pc = OpVariable %pc_block_ptr PushConstant
%pssbo_array = OpTypeArray %float %int_4
%pssbo_ptr = OpTypePointer PhysicalStorageBuffer %pssbo_array
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%pc_gep = OpAccessChain %pc_long_ptr %pc %int_0
%addr = OpLoad %long %pc_gep
%ptr = OpConvertUToPtr %pssbo_ptr %addr
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "decorated as Block for variable in PhysicalStorageBuffer storage "
          "class must follow relaxed storage buffer layout rules: member 0 "
          "contains an array with stride 0, but with an element size of 4"));
}

TEST_F(ValidateDecorations, MatrixArrayMissingMajorness) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
OpDecorate %array ArrayStride 32
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%block = OpTypeStruct %array
%ptr = OpTypePointer Uniform %block
%var = OpVariable %ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "must be explicitly laid out with RowMajor or ColMajor decorations"));
}

TEST_F(ValidateDecorations, MatrixArrayMissingStride) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpDecorate %array ArrayStride 32
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%block = OpTypeStruct %array
%ptr = OpTypePointer Uniform %block
%var = OpVariable %ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, MatrixArrayBadStride) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 0 MatrixStride 8
OpDecorate %array ArrayStride 32
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%block = OpTypeStruct %array
%ptr = OpTypePointer Uniform %block
%var = OpVariable %ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("is a matrix with stride 8 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, MatrixArrayArrayMissingMajorness) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 MatrixStride 16
OpDecorate %array ArrayStride 32
OpDecorate %rta ArrayStride 64
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%rta = OpTypeRuntimeArray %array
%block = OpTypeStruct %rta
%ptr = OpTypePointer StorageBuffer %block
%var = OpVariable %ptr StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "must be explicitly laid out with RowMajor or ColMajor decorations"));
}

TEST_F(ValidateDecorations, MatrixArrayArrayMissingStride) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpDecorate %array ArrayStride 32
OpDecorate %rta ArrayStride 64
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%rta = OpTypeRuntimeArray %array
%block = OpTypeStruct %rta
%ptr = OpTypePointer StorageBuffer %block
%var = OpVariable %ptr StorageBuffer
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("must be explicitly laid out with MatrixStride decorations"));
}

TEST_F(ValidateDecorations, MatrixArrayArrayBadStride) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 0 ColMajor
OpMemberDecorate %block 0 MatrixStride 8
OpDecorate %array ArrayStride 32
OpDecorate %a ArrayStride 64
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%vec = OpTypeVector %float 2
%mat = OpTypeMatrix %vec 2
%array = OpTypeArray %mat %int_2
%a = OpTypeArray %array %int_2
%block = OpTypeStruct %a
%ptr = OpTypePointer Uniform %block
%var = OpVariable %ptr Uniform
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("is a matrix with stride 8 not satisfying alignment to 16"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsInputVertex) {
  const std::string body = R"(
               OpCapability Shader
               OpCapability DrawParameters
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %_ %gl_BaseInstance1 %gl_BaseInstance2
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance
               OpDecorate %gl_PerVertex Block
               OpDecorate %gl_BaseInstance1 BuiltIn BaseInstance
               OpDecorate %gl_BaseInstance2 BuiltIn BaseInstance
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
%_arr_float_uint_1 = OpTypeArray %float %uint_1
%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
          %_ = OpVariable %_ptr_Output_gl_PerVertex Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %float_0 = OpConstant %float 0
         %17 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%_ptr_Input_int = OpTypePointer Input %int
%gl_BaseInstance1 = OpVariable %_ptr_Input_int Input
%gl_BaseInstance2 = OpVariable %_ptr_Input_int Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %main = OpFunction %void None %3
          %5 = OpLabel
         %20 = OpLoad %int %gl_BaseInstance1
         %21 = OpConvertSToF %float %20
         %22 = OpVectorTimesScalar %v4float %17 %21
         %24 = OpAccessChain %_ptr_Output_v4float %_ %int_0
               OpStore %24 %22
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpEntryPoint contains duplicate input variables with "
                        "BaseInstance builtin"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09658"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsInputMesh) {
  const std::string body = R"(
               OpCapability DrawParameters
               OpCapability MeshShadingEXT
               OpExtension "SPV_EXT_mesh_shader"
               OpMemoryModel Logical GLSL450
               OpEntryPoint MeshEXT %main "main" %gl_DrawID_1 %gl_DrawID_2
               OpExecutionMode %main LocalSize 1 1 1
               OpExecutionMode %main OutputVertices 32
               OpExecutionMode %main OutputPrimitivesEXT 32
               OpExecutionMode %main OutputTrianglesEXT
               OpDecorate %gl_DrawID_1 BuiltIn DrawIndex
               OpDecorate %gl_DrawID_2 BuiltIn DrawIndex
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
%_ptr_Input_int = OpTypePointer Input %int
  %gl_DrawID_1 = OpVariable %_ptr_Input_int Input
  %gl_DrawID_2 = OpVariable %_ptr_Input_int Input
       %uint = OpTypeInt 32 0
       %main = OpFunction %void None %3
          %5 = OpLabel
          %9 = OpLoad %int %gl_DrawID_1
         %11 = OpBitcast %uint %9
         %12 = OpLoad %int %gl_DrawID_2
         %13 = OpBitcast %uint %12
               OpSetMeshOutputsEXT %11 %13
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpEntryPoint contains duplicate input variables with "
                        "DrawIndex builtin"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09658"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsInputCompute) {
  const std::string body = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %_ %gl_WorkGroupID_1 %gl_WorkGroupID_2
               OpExecutionMode %main LocalSize 1 1 1
               OpMemberDecorate %Buffers 0 Offset 0
               OpDecorate %Buffers Block
               OpDecorate %_ DescriptorSet 0
               OpDecorate %_ Binding 0
               OpDecorate %gl_WorkGroupID_1 BuiltIn WorkgroupId
               OpDecorate %gl_WorkGroupID_2 BuiltIn WorkgroupId
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v3uint = OpTypeVector %uint 3
    %Buffers = OpTypeStruct %v3uint
%_ptr_StorageBuffer_Buffers = OpTypePointer StorageBuffer %Buffers
          %_ = OpVariable %_ptr_StorageBuffer_Buffers StorageBuffer
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_Input_v3uint = OpTypePointer Input %v3uint
%gl_WorkGroupID_1 = OpVariable %_ptr_Input_v3uint Input
%gl_WorkGroupID_2 = OpVariable %_ptr_Input_v3uint Input
%_ptr_StorageBuffer_v3uint = OpTypePointer StorageBuffer %v3uint
       %main = OpFunction %void None %3
          %5 = OpLabel
         %15 = OpLoad %v3uint %gl_WorkGroupID_1
         %16 = OpLoad %v3uint %gl_WorkGroupID_2
         %17 = OpIAdd %v3uint %15 %16
         %19 = OpAccessChain %_ptr_StorageBuffer_v3uint %_ %int_0
               OpStore %19 %17
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpEntryPoint contains duplicate input variables with "
                        "WorkgroupId builtin"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09658"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsOutputFragment) {
  const std::string body = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %gl_FragDepth_1 %gl_FragDepth_2
               OpExecutionMode %main OriginUpperLeft
               OpExecutionMode %main DepthReplacing
               OpDecorate %gl_FragDepth_1 BuiltIn FragDepth
               OpDecorate %gl_FragDepth_2 BuiltIn FragDepth
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
%_ptr_Output_float = OpTypePointer Output %float
%gl_FragDepth_1 = OpVariable %_ptr_Output_float Output
%gl_FragDepth_2 = OpVariable %_ptr_Output_float Output
    %float_1 = OpConstant %float 1
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpStore %gl_FragDepth_1 %float_1
         %10 = OpLoad %float %gl_FragDepth_1
         %11 = OpFAdd %float %10 %float_1
               OpStore %gl_FragDepth_2 %11
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("OpEntryPoint contains duplicate output variables with "
                        "FragDepth builtin"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09659"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsRayTmaxKHR) {
  const std::string body = R"(
               OpCapability RayTracingKHR
               OpExtension "SPV_KHR_ray_tracing"
               OpMemoryModel Logical GLSL450
               OpEntryPoint AnyHitKHR %main "main" %gl_RayTmaxEXT %gl_HitTEXT %incomingPayload
               OpDecorate %gl_RayTmaxEXT BuiltIn RayTmaxKHR
               OpDecorate %gl_HitTEXT BuiltIn RayTmaxKHR
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
%gl_RayTmaxEXT = OpVariable %_ptr_Input_float Input
 %gl_HitTEXT = OpVariable %_ptr_Input_float Input
    %v4float = OpTypeVector %float 4
%_ptr_IncomingRayPayloadKHR_v4float = OpTypePointer IncomingRayPayloadKHR %v4float
%incomingPayload = OpVariable %_ptr_IncomingRayPayloadKHR_v4float IncomingRayPayloadKHR
       %main = OpFunction %void None %3
          %5 = OpLabel
          %a = OpVariable %_ptr_Function_float Function
          %b = OpVariable %_ptr_Function_float Function
         %11 = OpLoad %float %gl_RayTmaxEXT
               OpStore %a %11
         %14 = OpLoad %float %gl_HitTEXT
               OpStore %b %14
         %18 = OpLoad %float %a
         %19 = OpLoad %float %b
         %22 = OpCompositeConstruct %v4float %18 %18 %19 %19
               OpStore %incomingPayload %22
               OpTerminateRayKHR
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_2);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "OpEntryPoint contains duplicate input variables with RayTmax"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09658"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsBlock) {
  const std::string body = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %var
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpMemberDecorate %gl_PerVertex 1 BuiltIn Position
               OpDecorate %gl_PerVertex Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%gl_PerVertex = OpTypeStruct %v4float %v4float
%_ptr_gl_PerVertex = OpTypePointer Output %gl_PerVertex
        %var = OpVariable %_ptr_gl_PerVertex Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
    %float_0 = OpConstant %float 0
         %17 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
   %ptr_vec4 = OpTypePointer Output %v4float
       %main = OpFunction %void None %3
          %5 = OpLabel
         %19 = OpAccessChain %ptr_vec4 %var %int_0
               OpStore %19 %17
         %22 = OpAccessChain %ptr_vec4 %var %int_1
               OpStore %22 %17
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "OpEntryPoint contains duplicate output variables with Position"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09659"));
}

TEST_F(ValidateDecorations, MultipleBuiltinsBlockMixed) {
  const std::string body = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %var %position
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpDecorate %gl_PerVertex Block
               OpDecorate %position BuiltIn Position
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%gl_PerVertex = OpTypeStruct %v4float
%_ptr_gl_PerVertex = OpTypePointer Output %gl_PerVertex
        %var = OpVariable %_ptr_gl_PerVertex Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
    %float_0 = OpConstant %float 0
         %17 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
   %ptr_vec4 = OpTypePointer Output %v4float
   %position = OpVariable %ptr_vec4 Output
       %main = OpFunction %void None %3
          %5 = OpLabel
         %19 = OpAccessChain %ptr_vec4 %var %int_0
               OpStore %19 %17
               OpStore %position %17
               OpReturn
               OpFunctionEnd
    )";

  CompileSuccessfully(body.c_str(), SPV_ENV_VULKAN_1_0);
  ASSERT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "OpEntryPoint contains duplicate output variables with Position"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpEntryPoint-09659"));
}

TEST_F(ValidateDecorations, UntypedVariableWorkgroupRequiresStruct) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %var
%void = OpTypeVoid
%int = OpTypeInt 32 0
%ptr = OpTypeUntypedPointerKHR Workgroup
%var = OpUntypedVariableKHR %ptr Workgroup %int
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Untyped workgroup variables in shaders must be block "
                        "decorated structs"));
}

TEST_F(ValidateDecorations, UntypedVariableWorkgroupRequiresBlockStruct) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %var
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR Workgroup
%var = OpUntypedVariableKHR %ptr Workgroup %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_UNIVERSAL_1_4));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Untyped workgroup variables in shaders must be block "
                        "decorated"));
}

TEST_F(ValidateDecorations, UntypedVariableStorageBufferMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %struct "struct"
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR StorageBuffer
%var = OpUntypedVariableKHR %ptr StorageBuffer %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("StorageBuffer id '2' is missing Block decoration"));
}

TEST_F(ValidateDecorations, UntypedVariableUniformMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %struct "struct"
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR Uniform
%var = OpUntypedVariableKHR %ptr Uniform %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Uniform id '2' is missing Block or BufferBlock decoration"));
}

TEST_F(ValidateDecorations, UntypedVariablePushConstantMissingBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %struct "struct"
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR PushConstant
%var = OpUntypedVariableKHR %ptr PushConstant %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("PushConstant id '2' is missing Block decoration"));
}

using UntypedVariableSetAndBinding = spvtest::ValidateBase<std::string>;

TEST_P(UntypedVariableSetAndBinding, MissingSet) {
  const auto sc = GetParam();
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %var "var"
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR )" +
                            sc + R"(
%var = OpUntypedVariableKHR %ptr )" + sc + R"( %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%load = OpLoad %struct %var
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr(sc + " id '2' is missing DescriptorSet decoration"));
}

TEST_P(UntypedVariableSetAndBinding, MissingBinding) {
  const auto sc = GetParam();
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpName %var "var"
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %var DescriptorSet 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%ptr = OpTypeUntypedPointerKHR )" +
                            sc + R"(
%var = OpUntypedVariableKHR %ptr )" + sc + R"( %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%load = OpLoad %struct %var
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr(sc + " id '2' is missing Binding decoration"));
}

INSTANTIATE_TEST_SUITE_P(ValidateUntypedVariableSetAndBinding,
                         UntypedVariableSetAndBinding,
                         Values("StorageBuffer", "Uniform"));

using UntypedPointerLayout =
    spvtest::ValidateBase<std::tuple<std::string, std::string>>;

TEST_P(UntypedPointerLayout, BadOffset) {
  const auto sc = std::get<0>(GetParam());
  const auto op = std::get<1>(GetParam());
  const std::string set = (sc == "StorageBuffer" || sc == "Uniform"
                               ? R"(OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
)"
                               : R"()");
  const std::string spirv = R"(
OpCapability Shader
OpCapability VariablePointers
OpCapability UntypedPointersKHR
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %var
OpExecutionMode %main LocalSize 1 1 1
OpName %var "var"
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpMemberDecorate %struct 1 Offset 4
)" + set + R"(OpMemberDecorate %test_type 0 Offset 0
OpMemberDecorate %test_type 1 Offset 1
OpDecorate %ptr ArrayStride 16
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_0 = OpConstant %int 0
%struct = OpTypeStruct %int %int
%test_type = OpTypeStruct %int %int
%test_val = OpConstantNull %test_type
%ptr = OpTypeUntypedPointerKHR )" +
                            sc + R"(
%var = OpUntypedVariableKHR %ptr )" +
                            sc + R"( %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
)" + op + R"(
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  const bool read_only = sc == "Uniform" || sc == "PushConstant";
  if (!read_only || op.find("OpStore") == std::string::npos) {
    EXPECT_THAT(getDiagnosticString(),
                HasSubstr("member 1 at offset 1 is not aligned to"));
  }
}

TEST_P(UntypedPointerLayout, BadStride) {
  const auto sc = std::get<0>(GetParam());
  const auto op = std::get<1>(GetParam());
  const std::string set = (sc == "StorageBuffer" || sc == "Uniform"
                               ? R"(OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
)"
                               : R"()");
  const std::string spirv = R"(
OpCapability Shader
OpCapability VariablePointers
OpCapability UntypedPointersKHR
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_variable_pointers"
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %var
OpExecutionMode %main LocalSize 1 1 1
OpName %var "var"
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpMemberDecorate %struct 1 Offset 4
)" + set + R"(OpDecorate %test_type ArrayStride 4
OpDecorate %ptr ArrayStride 16
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_0 = OpConstant %int 0
%int_4 = OpConstant %int 4
%int4 = OpTypeVector %int 4
%test_type = OpTypeArray %int4 %int_4
%test_val = OpConstantNull %test_type
%struct = OpTypeStruct %int %int
%ptr = OpTypeUntypedPointerKHR )" +
                            sc + R"(
%var = OpUntypedVariableKHR %ptr )" +
                            sc + R"( %struct
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
)" + op + R"(
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  const bool read_only = sc == "Uniform" || sc == "PushConstant";
  if (!read_only || op.find("OpStore") == std::string::npos) {
    EXPECT_THAT(
        getDiagnosticString(),
        HasSubstr("array with stride 4 not satisfying alignment to 16"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ValidateUntypedPointerLayout, UntypedPointerLayout,
    Combine(Values("StorageBuffer", "Uniform", "PushConstant", "Workgroup"),
            Values("%gep = OpUntypedAccessChainKHR %ptr %test_type %var %int_0",
                   "%gep = OpUntypedInBoundsAccessChainKHR %ptr %test_type "
                   "%var %int_0",
                   "%gep = OpUntypedPtrAccessChainKHR %ptr %test_type %var "
                   "%int_0 %int_0",
                   "%ld = OpLoad %test_type %var", "OpStore %var %test_val")));

TEST_F(ValidateDecorations, UntypedArrayLengthMissingOffset) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpDecorate %array ArrayStride 4
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%array = OpTypeRuntimeArray %int
%struct = OpTypeStruct %array
%block = OpTypeStruct %array
%ptr = OpTypeUntypedPointerKHR StorageBuffer
%var = OpUntypedVariableKHR %ptr StorageBuffer %block
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%len = OpUntypedArrayLengthKHR %int %struct %var 0
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_2);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_2));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("member 0 is missing an Offset decoration"));
}

TEST_F(ValidateDecorations, ComponentMultipleArrays) {
  const std::string spirv = R"(
               OpCapability Tessellation
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationEvaluation %main "main" %_ %FOO %FOO0
               OpExecutionMode %main Triangles
               OpExecutionMode %main SpacingEqual
               OpExecutionMode %main VertexOrderCcw
               OpSource GLSL 460
               OpSourceExtension "GL_EXT_nonuniform_qualifier"
               OpName %main "main"
               OpName %gl_PerVertex "gl_PerVertex"
               OpMemberName %gl_PerVertex 0 "gl_Position"
               OpMemberName %gl_PerVertex 1 "gl_PointSize"
               OpMemberName %gl_PerVertex 2 "gl_ClipDistance"
               OpMemberName %gl_PerVertex 3 "gl_CullDistance"
               OpName %_ ""
               OpName %FOO "FOO"
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance
               OpDecorate %gl_PerVertex Block
               OpDecorate %FOO Component 2
               OpDecorate %FOO Location 1
               OpDecorate %FOO0 Location 1
               OpDecorate %FOO0 Component 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
%_arr_float_uint_1 = OpTypeArray %float %uint_1
%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
          %_ = OpVariable %_ptr_Output_gl_PerVertex Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %v2float = OpTypeVector %float 2
     %uint_2 = OpConstant %uint 2
%_arr_v2float_uint_2 = OpTypeArray %v2float %uint_2
    %uint_32 = OpConstant %uint 32
%_arr__arr_v2float_uint_2_uint_32 = OpTypeArray %_arr_v2float_uint_2 %uint_32
%_ptr_Input__arr__arr_v2float_uint_2_uint_32 = OpTypePointer Input %_arr__arr_v2float_uint_2_uint_32
        %FOO = OpVariable %_ptr_Input__arr__arr_v2float_uint_2_uint_32 Input
        %FOO0 = OpVariable %_ptr_Input__arr__arr_v2float_uint_2_uint_32 Input
%_ptr_Input_v2float = OpTypePointer Input %v2float
      %int_1 = OpConstant %int 1
     %uint_0 = OpConstant %uint 0
%_ptr_Output_float = OpTypePointer Output %float
       %main = OpFunction %void None %3
          %5 = OpLabel
         %24 = OpAccessChain %_ptr_Input_v2float %FOO %int_0 %int_0
         %25 = OpLoad %v2float %24
         %27 = OpAccessChain %_ptr_Input_v2float %FOO0 %int_1 %int_1
         %28 = OpLoad %v2float %27
         %29 = OpFAdd %v2float %25 %28
         %32 = OpAccessChain %_ptr_Output_float %_ %int_0 %uint_0
         %33 = OpCompositeExtract %float %29 0
               OpStore %32 %33
         %34 = OpAccessChain %_ptr_Output_float %_ %int_0 %uint_1
         %35 = OpCompositeExtract %float %29 1
               OpStore %34 %35
               OpReturn
               OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

const std::string kNodeShaderPrelude = R"(
OpCapability Shader
OpCapability ShaderEnqueueAMDX
OpExtension "SPV_AMDX_shader_enqueue"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpEntryPoint GLCompute %other "other"
)";

const std::string kNodeShaderPostlude = R"(
%uint = OpTypeInt 32 0
%uint_0 = OpConstant %uint 0
%uint_1 = OpConstant %uint 1
%node0 = OpConstantStringAMDX "node0"
%node1 = OpConstantStringAMDX "node1"
%node2 = OpConstantStringAMDX "node2"
%S = OpTypeStruct
%_payloadarr_S_0 = OpTypeNodePayloadArrayAMDX %S
%_payloadarr_S = OpTypeNodePayloadArrayAMDX %S
%bool = OpTypeBool
%true = OpConstantTrue %bool
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
%other = OpFunction %void None %void_fn
%entry0 = OpLabel
OpReturn
OpFunctionEnd
)";

TEST_F(ValidateDecorations, NodeShader) {
  const std::string spirv = kNodeShaderPrelude + R"(
OpExecutionModeId %main ShaderIndexAMDX %uint_0
OpExecutionModeId %main IsApiEntryAMDX %true
OpExecutionModeId %main MaxNodeRecursionAMDX %uint_1
OpExecutionModeId %main MaxNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpExecutionModeId %main SharesInputWithAMDX %node0 %uint_0
OpExecutionModeId %other ShaderIndexAMDX %uint_0
OpExecutionModeId %other StaticNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpDecorateId %_payloadarr_S PayloadNodeNameAMDX %node1
OpDecorateId %_payloadarr_S_0 PayloadNodeNameAMDX %node2
OpDecorateId %_payloadarr_S PayloadNodeBaseIndexAMDX %uint_0
OpDecorateId %_payloadarr_S PayloadNodeArraySizeAMDX %uint_1
OpDecorateId %_payloadarr_S NodeSharesPayloadLimitsWithAMDX %_payloadarr_S_0
)" + kNodeShaderPostlude;

  spv_target_env env = SPV_ENV_UNIVERSAL_1_3;
  CompileSuccessfully(spirv, env);
  EXPECT_THAT(SPV_SUCCESS, ValidateInstructions(env));
}

TEST_F(ValidateDecorations, NodeShaderDecoratePayloadNodeName) {
  const std::string spirv = kNodeShaderPrelude + R"(
OpExecutionModeId %main ShaderIndexAMDX %uint_0
OpExecutionModeId %main IsApiEntryAMDX %true
OpExecutionModeId %main MaxNodeRecursionAMDX %uint_1
OpExecutionModeId %main MaxNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpExecutionModeId %main SharesInputWithAMDX %node0 %uint_0
OpExecutionModeId %other ShaderIndexAMDX %uint_0
OpExecutionModeId %other StaticNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpDecorate %_payloadarr_S PayloadNodeNameAMDX %node1
OpDecorate %_payloadarr_S_0 PayloadNodeNameAMDX %node2
OpDecorateId %_payloadarr_S PayloadNodeBaseIndexAMDX %uint_0
OpDecorateId %_payloadarr_S PayloadNodeArraySizeAMDX %uint_1
OpDecorateId %_payloadarr_S NodeSharesPayloadLimitsWithAMDX %_payloadarr_S_0
)" + kNodeShaderPostlude;

  spv_target_env env = SPV_ENV_UNIVERSAL_1_3;
  CompileSuccessfully(spirv, env);
  EXPECT_THAT(SPV_ERROR_INVALID_ID, ValidateInstructions(env));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Decorations taking ID parameters may not be used with OpDecorate"));
}

TEST_F(ValidateDecorations, NodeShaderDecoratePayloadNodeBaseIndex) {
  const std::string spirv = kNodeShaderPrelude + R"(
OpExecutionModeId %main ShaderIndexAMDX %uint_0
OpExecutionModeId %main IsApiEntryAMDX %true
OpExecutionModeId %main MaxNodeRecursionAMDX %uint_1
OpExecutionModeId %main MaxNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpExecutionModeId %main SharesInputWithAMDX %node0 %uint_0
OpExecutionModeId %other ShaderIndexAMDX %uint_0
OpExecutionModeId %other StaticNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpDecorateId %_payloadarr_S PayloadNodeNameAMDX %node1
OpDecorateId %_payloadarr_S_0 PayloadNodeNameAMDX %node2
OpDecorate %_payloadarr_S PayloadNodeBaseIndexAMDX %uint_0
OpDecorateId %_payloadarr_S PayloadNodeArraySizeAMDX %uint_1
OpDecorateId %_payloadarr_S NodeSharesPayloadLimitsWithAMDX %_payloadarr_S_0
)" + kNodeShaderPostlude;

  spv_target_env env = SPV_ENV_UNIVERSAL_1_3;
  CompileSuccessfully(spirv, env);
  EXPECT_THAT(SPV_ERROR_INVALID_ID, ValidateInstructions(env));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Decorations taking ID parameters may not be used with OpDecorate"));
}

TEST_F(ValidateDecorations, NodeShaderDecoratePayloadNodeArraySize) {
  const std::string spirv = kNodeShaderPrelude + R"(
OpExecutionModeId %main ShaderIndexAMDX %uint_0
OpExecutionModeId %main IsApiEntryAMDX %true
OpExecutionModeId %main MaxNodeRecursionAMDX %uint_1
OpExecutionModeId %main MaxNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpExecutionModeId %main SharesInputWithAMDX %node0 %uint_0
OpExecutionModeId %other ShaderIndexAMDX %uint_0
OpExecutionModeId %other StaticNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpDecorateId %_payloadarr_S PayloadNodeNameAMDX %node1
OpDecorateId %_payloadarr_S_0 PayloadNodeNameAMDX %node2
OpDecorateId %_payloadarr_S PayloadNodeBaseIndexAMDX %uint_0
OpDecorate %_payloadarr_S PayloadNodeArraySizeAMDX %uint_1
OpDecorateId %_payloadarr_S NodeSharesPayloadLimitsWithAMDX %_payloadarr_S_0
)" + kNodeShaderPostlude;

  spv_target_env env = SPV_ENV_UNIVERSAL_1_3;
  CompileSuccessfully(spirv, env);
  EXPECT_THAT(SPV_ERROR_INVALID_ID, ValidateInstructions(env));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Decorations taking ID parameters may not be used with OpDecorate"));
}

TEST_F(ValidateDecorations, NodeShaderDecorateNodeSharesPayloadLimitsWith) {
  const std::string spirv = kNodeShaderPrelude + R"(
OpExecutionModeId %main ShaderIndexAMDX %uint_0
OpExecutionModeId %main IsApiEntryAMDX %true
OpExecutionModeId %main MaxNodeRecursionAMDX %uint_1
OpExecutionModeId %main MaxNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpExecutionModeId %main SharesInputWithAMDX %node0 %uint_0
OpExecutionModeId %other ShaderIndexAMDX %uint_0
OpExecutionModeId %other StaticNumWorkgroupsAMDX %uint_1 %uint_1 %uint_1
OpDecorateId %_payloadarr_S PayloadNodeNameAMDX %node1
OpDecorateId %_payloadarr_S_0 PayloadNodeNameAMDX %node2
OpDecorateId %_payloadarr_S PayloadNodeBaseIndexAMDX %uint_0
OpDecorateId %_payloadarr_S PayloadNodeArraySizeAMDX %uint_1
OpDecorate %_payloadarr_S NodeSharesPayloadLimitsWithAMDX %_payloadarr_S_0
)" + kNodeShaderPostlude;

  spv_target_env env = SPV_ENV_UNIVERSAL_1_3;
  CompileSuccessfully(spirv, env);
  EXPECT_THAT(SPV_ERROR_INVALID_ID, ValidateInstructions(env));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr(
          "Decorations taking ID parameters may not be used with OpDecorate"));
}

TEST_F(ValidateDecorations, BlockArrayWithStride) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array ArrayStride 4
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%struct = OpTypeStruct %int
%array = OpTypeArray %struct %int_4
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Array containing a Block or BufferBlock must not be "
                        "decorated with ArrayStride"));
}

TEST_F(ValidateDecorations, BufferBlockRuntimeArrayWithStride) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability Linkage
OpMemoryModel Logical GLSL450
OpDecorate %struct BufferBlock
OpMemberDecorate %struct 0 Offset 0
OpDecorate %array ArrayStride 4
%int = OpTypeInt 32 0
%struct = OpTypeStruct %int
%array = OpTypeRuntimeArray %struct
)";

  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Array containing a Block or BufferBlock must not be "
                        "decorated with ArrayStride"));
}

TEST_F(ValidateDecorations, BlockArrayWithoutStride) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%struct = OpTypeStruct %int
%array = OpTypeArray %struct %int_4
%ptr = OpTypePointer StorageBuffer %array
%var = OpVariable %ptr StorageBuffer
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, BlockArrayWithoutStrideUntypedAccessChain) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpExtension "SPV_KHR_untyped_pointers"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %struct Block
OpMemberDecorate %struct 0 Offset 0
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%struct = OpTypeStruct %int
%array = OpTypeArray %struct %int_4
%void = OpTypeVoid
%ptr = OpTypeUntypedPointerKHR StorageBuffer
%var = OpUntypedVariableKHR %ptr StorageBuffer %array
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%gep = OpUntypedAccessChainKHR %ptr %array %var
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, InvalidLayoutBlockFunctionPre1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%ptr_function_block = OpTypePointer Function %block
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%var = OpVariable %ptr_function_block Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_4);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateDecorations, InvalidLayoutBlockFunctionPost1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%ptr_function_block = OpTypePointer Function %block
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%var = OpVariable %ptr_function_block Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_5);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutOffsetPrivatePre1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpMemberDecorate %block 0 Offset 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%ptr_private_block = OpTypePointer Private %block
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_private_block Private
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, InvalidLayoutOffsetPrivatePost1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpMemberDecorate %block 0 Offset 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%ptr_private_block = OpTypePointer Private %block
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_private_block Private
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutArrayStrideWorkgroupExplicitLayout) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability WorkgroupMemoryExplicitLayoutKHR
OpExtension "SPV_KHR_workgroup_memory_explicit_layout"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %array ArrayStride 4
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%array = OpTypeArray %int %int_4
%ptr_wg_block = OpTypePointer Workgroup %array
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_wg_block Workgroup
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_3));
}

TEST_F(ValidateDecorations, InvalidLayoutArrayStrideWorkgroup) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %array ArrayStride 4
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%array = OpTypeArray %int %int_4
%ptr_wg_block = OpTypePointer Workgroup %array
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_wg_block Workgroup
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutArrayStrideUniformConstant) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %array ArrayStride 4
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%int_4 = OpConstant %int 4
%sampler = OpTypeSampler
%array = OpTypeArray %sampler %int_4
%ptr_uc_block = OpTypePointer UniformConstant %array
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_uc_block UniformConstant
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutMatrixStrideFunctionPost1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpMemberDecorate %block 0 MatrixStride 16
%void = OpTypeVoid
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%mat4x4 = OpTypeMatrix %v4float 4
%block = OpTypeStruct %mat4x4
%ptr_function_block = OpTypePointer Function %block
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%var = OpVariable %ptr_function_block Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutNestedMatrixStrideFunctionPost1p4) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpMemberDecorate %block 0 MatrixStride 16
%void = OpTypeVoid
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%mat4x4 = OpTypeMatrix %v4float 4
%block = OpTypeStruct %mat4x4
%block2 = OpTypeStruct %block
%int = OpTypeInt 32 0
%int_2 = OpConstant %int 2
%array = OpTypeArray %block2 %int_2
%ptr_function_array = OpTypePointer Function %array
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
%var = OpVariable %ptr_function_array Function
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_3);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_3));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutBufferBlockWorkgroup) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block BufferBlock
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%ptr_wg_block = OpTypePointer Workgroup %block
%void_fn = OpTypeFunction %void
%var = OpVariable %ptr_wg_block Workgroup
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-10684"));
  EXPECT_THAT(
      getDiagnosticString(),
      HasSubstr("Invalid explicit layout decorations on type for operand"));
}

TEST_F(ValidateDecorations, InvalidLayoutUntypedStore) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability UntypedPointersKHR
OpExtension "SPV_KHR_untyped_pointers"
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
%void = OpTypeVoid
%int = OpTypeInt 32 0
%block = OpTypeStruct %int
%block_null = OpConstantNull %block
%ptr = OpTypeUntypedPointerKHR StorageBuffer
%var = OpUntypedVariableKHR %ptr StorageBuffer %block
%void_fn = OpTypeFunction %void
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpStore %var %block_null
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_0));
}

TEST_F(ValidateDecorations, ExplicitLayoutOnPtrPhysicalStorageBuffer) {
  const std::string spirv = R"(
OpCapability PhysicalStorageBufferAddresses
OpCapability Int64
OpCapability Shader
OpExtension "SPV_KHR_physical_storage_buffer"
OpMemoryModel PhysicalStorageBuffer64 GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %_ptr_PhysicalStorageBuffer_int ArrayStride 4
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%int = OpTypeInt 32 1
%_ptr_PhysicalStorageBuffer_int = OpTypePointer PhysicalStorageBuffer %int  ; ArrayStride 4
%Foo = OpTypeStruct %_ptr_PhysicalStorageBuffer_int
%_ptr_Function_Foo = OpTypePointer Function %Foo
%int_0 = OpConstant %int 0
%_ptr_Function__ptr_PhysicalStorageBuffer_int = OpTypePointer Function %_ptr_PhysicalStorageBuffer_int
%ulong = OpTypeInt 64 0
%ulong_0 = OpConstant %ulong 0
%main = OpFunction %void None %void_fn
%entry = OpLabel
%obj = OpVariable %_ptr_Function_Foo Function
%obj_member = OpAccessChain %_ptr_Function__ptr_PhysicalStorageBuffer_int %obj %int_0
%nullptr = OpConvertUToPtr %_ptr_PhysicalStorageBuffer_int %ulong_0
OpStore %obj_member %nullptr
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_UNIVERSAL_1_5);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_2));
}

TEST_F(ValidateDecorations, RuntimeArrayNotLargestOffsetInBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 16
OpMemberDecorate %block 1 Offset 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%int = OpTypeInt 32 0
%array = OpTypeRuntimeArray %int
%block = OpTypeStruct %int %array
%ptr = OpTypePointer StorageBuffer %block
%var = OpVariable %ptr StorageBuffer
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("has a runtime array at offset 0, but other members at "
                        "larger offsets"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpTypeRuntimeArray-04680"));
}

TEST_F(ValidateDecorations, RuntimeArrayNotLargestOffsetInBufferBlock) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %var DescriptorSet 0
OpDecorate %var Binding 0
OpDecorate %block BufferBlock
OpMemberDecorate %block 0 Offset 16
OpMemberDecorate %block 1 Offset 0
%void = OpTypeVoid
%void_fn = OpTypeFunction %void
%int = OpTypeInt 32 0
%array = OpTypeRuntimeArray %int
%block = OpTypeStruct %int %array
%ptr = OpTypePointer Uniform %block
%var = OpVariable %ptr Uniform
%main = OpFunction %void None %void_fn
%entry = OpLabel
OpReturn
OpFunctionEnd
)";

  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_0);
  EXPECT_EQ(SPV_ERROR_INVALID_ID, ValidateInstructions(SPV_ENV_VULKAN_1_0));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("has a runtime array at offset 0, but other members at "
                        "larger offsets"));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-OpTypeRuntimeArray-04680"));
}

}  // namespace
}  // namespace val
}  // namespace spvtools
