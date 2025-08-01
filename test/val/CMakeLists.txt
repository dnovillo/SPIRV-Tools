# Copyright (c) 2016 The Khronos Group Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(VAL_TEST_COMMON_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../test_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../unit_spirv.h
  ${CMAKE_CURRENT_SOURCE_DIR}/val_code_generator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/val_code_generator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/val_fixtures.h
)

add_spvtools_unittest(TARGET val_abcde
  SRCS
       val_adjacency_test.cpp
       val_annotation_test.cpp
       val_arithmetics_test.cpp
       val_atomics_test.cpp
       val_barriers_test.cpp
       val_bitwise_test.cpp
       val_builtins_test.cpp
       val_cfg_test.cpp
       val_composites_test.cpp
       val_constants_test.cpp
       val_conversion_test.cpp
       val_data_test.cpp
       val_decoration_test.cpp
       val_derivatives_test.cpp
       val_entry_point_test.cpp
       val_explicit_reserved_test.cpp
       val_invalid_type_test.cpp
       val_extensions_test.cpp
       val_extension_spv_khr_expect_assume_test.cpp
       val_extension_spv_khr_linkonce_odr_test.cpp
       val_extension_spv_khr_subgroup_uniform_control_flow_test.cpp
       val_extension_spv_khr_integer_dot_product_test.cpp
       val_extension_spv_khr_bit_instructions_test.cpp
       val_extension_spv_khr_terminate_invocation_test.cpp
       val_extension_spv_khr_subgroup_rotate_test.cpp
       val_extension_spv_nv_raw_access_chains.cpp
       val_extension_spv_intel_inline_assembly.cpp
       val_ext_inst_test.cpp
       val_ext_inst_debug_test.cpp
       ${VAL_TEST_COMMON_SRCS}
  LIBS ${SPIRV_TOOLS_FULL_VISIBILITY}
  PCH_FILE pch_test_val
)

add_spvtools_unittest(TARGET val_capability
  SRCS
       val_capability_test.cpp
  LIBS ${SPIRV_TOOLS_FULL_VISIBILITY}
  PCH_FILE pch_test_val
)

add_spvtools_unittest(TARGET val_limits
  SRCS val_limits_test.cpp
       ${VAL_TEST_COMMON_SRCS}
  LIBS ${SPIRV_TOOLS_FULL_VISIBILITY}
  PCH_FILE pch_test_val
)

add_spvtools_unittest(TARGET val_fghijklmnop
  SRCS
       val_function_test.cpp
       val_id_test.cpp
       val_image_test.cpp
       val_interfaces_test.cpp
       val_layout_test.cpp
       val_literals_test.cpp
       val_logicals_test.cpp
       val_memory_test.cpp
       val_mesh_shading_test.cpp
       val_misc_test.cpp
       val_modes_test.cpp
       val_non_semantic_test.cpp
       val_non_uniform_test.cpp
       val_opencl_test.cpp
       val_primitives_test.cpp
       ${VAL_TEST_COMMON_SRCS}
  LIBS ${SPIRV_TOOLS_FULL_VISIBILITY}
  PCH_FILE pch_test_val
)

add_spvtools_unittest(TARGET val_rstuvw
  SRCS
       val_ray_query_test.cpp
       val_ray_tracing_test.cpp
       val_ray_tracing_reorder_test.cpp
       val_small_type_uses_test.cpp
       val_ssa_test.cpp
       val_state_test.cpp
       val_storage_test.cpp
       val_tensor_test.cpp
       val_type_unique_test.cpp
       val_validation_state_test.cpp
       val_version_test.cpp
       ${VAL_TEST_COMMON_SRCS}
  LIBS ${SPIRV_TOOLS_FULL_VISIBILITY}
  PCH_FILE pch_test_val
)
