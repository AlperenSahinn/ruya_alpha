@echo off

set SLANGC=slangc.exe

set FLAGS=-profile glsl_460 -target spirv -emit-spirv-directly -matrix-layout-column-major -I.

set CAPS=-capability spvRayTracingKHR

if not exist compiled mkdir compiled

echo Shader compilation starting...
echo.

%SLANGC% gbuffer.slang %FLAGS% -entry vertexMain -stage vertex -o compiled\gbuffer_vertex.spv
%SLANGC% gbuffer.slang %FLAGS% -entry fragmentMain -stage fragment -o compiled\gbuffer_fragment.spv

%SLANGC% rt_directional_light_shadow.slang %FLAGS% %CAPS% -entry rayGenMain -stage raygeneration -o compiled\rt_directional_light_shadow_ray_generation.spv
%SLANGC% rt_directional_light_shadow.slang %FLAGS% %CAPS% -entry missMain -stage miss -o compiled\rt_directional_light_shadow_ray_miss.spv

%SLANGC% pt_diffuse_global_illumination.slang %FLAGS% %CAPS% -entry rayGenMain -stage raygeneration -o compiled\pt_diffuse_global_illumination_ray_generation.spv
%SLANGC% pt_diffuse_global_illumination.slang %FLAGS% %CAPS% -entry missMain -stage miss -o compiled\pt_diffuse_global_illumination_ray_miss.spv
%SLANGC% pt_diffuse_global_illumination.slang %FLAGS% %CAPS% -entry shadowMissMain -stage miss -o compiled\pt_diffuse_global_illumination_shadow_ray_miss.spv
%SLANGC% pt_diffuse_global_illumination.slang %FLAGS% %CAPS% -entry closestHitMain -stage closesthit -o compiled\pt_diffuse_global_illumination_ray_closest_hit.spv

%SLANGC% r8_upsampling_compute.slang %FLAGS% -entry computeMain -stage compute -o compiled\r8_upsampling_compute.spv
%SLANGC% rgba32f_upsampling_compute.slang %FLAGS% -entry computeMain -stage compute -o compiled\rgba32f_upsampling_compute.spv

%SLANGC% bilateral_blur_shadow.slang %FLAGS% -entry computeMain -stage compute -o compiled\bilateral_blur_shadow.spv

%SLANGC% light_pass_compute.slang %FLAGS% -entry computeMain -stage compute -o compiled\light_pass_compute_shader.spv
%SLANGC% taa_compute.slang %FLAGS% -entry computeMain -stage compute -o compiled\taa_compute.spv
%SLANGC% tone_map_compute.slang %FLAGS% -entry computeMain -stage compute -o compiled\tone_map_compute.spv

%SLANGC% restir_temporal_reuse.slang %FLAGS% -entry computeMain -stage compute -o compiled\restir_temporal_reuse.spv
%SLANGC% restir_spatial_reuse.slang %FLAGS% -entry computeMain -stage compute -o compiled\restir_spatial_reuse.spv
%SLANGC% restir_resolve.slang %FLAGS% -entry computeMain -stage compute -o compiled\restir_resolve.spv

%SLANGC% diffuse_gi_temporal_reuse.slang %FLAGS% -entry computeMain -stage compute -o compiled\diffuse_gi_temporal_reuse.spv
%SLANGC% diffuse_gi_spatial_reuse.slang %FLAGS% -entry computeMain -stage compute -o compiled\diffuse_gi_spatial_reuse.spv
%SLANGC% diffuse_gi_sh4_resolve.slang %FLAGS% -entry computeMain -stage compute -o compiled\diffuse_gi_sh4_resolve.spv

%SLANGC% debug_line.slang %FLAGS% -entry vertexMain -stage vertex -o compiled\debug_line_vertex_shader.spv
%SLANGC% debug_line.slang %FLAGS% -entry fragmentMain -stage fragment -o compiled\debug_line_fragment_shader.spv

echo ##################################
echo.
echo Shaders successfully compiled.
echo.
pause
