#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_UPSCALER_XESS_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_UPSCALER_XESS_H_

#include "graphics/host_gpu/renderer/upscaler/dlss.h"

namespace Libs::Graphics::Xess {

struct Context;

using Result = int32_t;

inline constexpr Result RESULT_SUCCESS = 0;

enum class Quality : int32_t {
	UltraPerformance = 100,
	Performance      = 101,
	Balanced         = 102,
	Quality          = 103,
	UltraQuality     = 104,
	UltraQualityPlus = 105,
	NativeAa         = 106,
};

enum class NetworkModel : int32_t {
	Auto = 0,
	Xmx  = 1,
	Dp4a = 2,
};

struct Coord2D {
	uint32_t x;
	uint32_t y;
};

struct Version {
	uint16_t major;
	uint16_t minor;
	uint16_t patch;
	uint16_t reserved;
};

struct ImageViewInfo {
	VkImageView             image_view;
	VkImage                 image;
	VkImageSubresourceRange subresource_range;
	VkFormat                format;
	uint32_t                width;
	uint32_t                height;
};

struct InitParams {
	Coord2D         output_resolution;
	Quality         quality_setting;
	uint32_t        init_flags;
	uint32_t        creation_node_mask;
	uint32_t        visible_node_mask;
	VkDeviceMemory  temp_buffer_heap;
	uint64_t        buffer_heap_offset;
	VkDeviceMemory  temp_texture_heap;
	uint64_t        texture_heap_offset;
	VkPipelineCache pipeline_cache;
};

struct ExecuteParams {
	ImageViewInfo color_texture;
	ImageViewInfo velocity_texture;
	ImageViewInfo depth_texture;
	ImageViewInfo exposure_scale_texture;
	ImageViewInfo responsive_pixel_mask_texture;
	ImageViewInfo output_texture;
	float         jitter_offset_x;
	float         jitter_offset_y;
	float         exposure_scale;
	uint32_t      reset_history;
	uint32_t      input_width;
	uint32_t      input_height;
	Coord2D       input_color_base;
	Coord2D       input_motion_vector_base;
	Coord2D       input_depth_base;
	Coord2D       input_responsive_mask_base;
	Coord2D       reserved0;
	Coord2D       output_color_base;
};

static_assert(sizeof(Coord2D) == 8);
static_assert(sizeof(Version) == 8);
static_assert(sizeof(ImageViewInfo) == 48);
static_assert(sizeof(InitParams) == 64);
static_assert(sizeof(ExecuteParams) == 360);

inline constexpr uint32_t INIT_FLAG_INVERTED_DEPTH         = 1u << 0;
inline constexpr uint32_t INIT_FLAG_ENABLE_AUTOEXPOSURE    = 1u << 1;
inline constexpr uint32_t INIT_FLAG_EXPOSURE_SCALE_TEXTURE = 1u << 2;
inline constexpr uint32_t INIT_FLAG_RESPONSIVE_PIXEL_MASK  = 1u << 3;

[[nodiscard]] bool Reads(const Pssr::Request& request, const Dlss::Inputs& inputs);
[[nodiscard]] std::optional<Dlss::Creation> Choose(const Pssr::Request& request,
                                                   const Dlss::Inputs&  inputs);
[[nodiscard]] Quality DetermineQuality(uint32_t input_width, uint32_t output_width);
void SetCreation(InitParams& params, const Dlss::Creation& creation);
void SetExecute(ExecuteParams& params, const Pssr::Request& request,
                const Dlss::Inputs& inputs, bool reset);

void Load();
void Init(const GraphicContext& graphics);
void               Shutdown();
[[nodiscard]] bool Enabled();
[[nodiscard]] NetworkModel DetectedModel();

[[nodiscard]] Context* CreateContext(const Dlss::Creation& creation);
[[nodiscard]] bool Evaluate(vk::CommandBuffer command, Context* context,
                            const Pssr::Request& request, const Dlss::Inputs& inputs, bool reset);
void               ReleaseContext(Context* context);

}

#endif
