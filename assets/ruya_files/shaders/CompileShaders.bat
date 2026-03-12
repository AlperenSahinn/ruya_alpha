C:\VulkanSDK\1.4.341.1\Bin\glslang.exe -V -S vert -I./ --target-env vulkan1.4 gbuffer_vertex_shader.vert -o compiled\gbuffer_vertex_shader.spv

C:\VulkanSDK\1.4.341.1\Bin\glslang.exe -V -S frag -I./ --target-env vulkan1.4 gbuffer_fragment_shader.frag -o compiled\gbuffer_fragment_shader.spv

C:\VulkanSDK\1.4.341.1\Bin\glslang.exe -V -S rgen -I./ --target-env vulkan1.4 rt_directional_light_shadow_ray_generation.rgen -o compiled\rt_directional_light_shadow_ray_generation.spv
C:\VulkanSDK\1.4.341.1\Bin\glslang.exe -V -S rmiss -I./ --target-env vulkan1.4 rt_directional_light_shadow_ray_miss.rmiss -o compiled\rt_directional_light_shadow_ray_miss.spv

C:\VulkanSDK\1.4.341.1\Bin\glslang.exe -V -S comp -I./ --target-env vulkan1.4 light_pass_compute_shader.comp -o compiled\light_pass_compute_shader.spv

pause