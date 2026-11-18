#include "common/emulatorConfig.h"
#include "graphics/guest_gpu/pssr.h"
#include "graphics/host_gpu/renderer/upscaler/xess.h"

#include <magic_enum.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string_view>

namespace {

static_assert(sizeof(Libs::Graphics::Xess::Coord2D) == 8, "sizeof(Coord2D) mismatch");
static_assert(sizeof(Libs::Graphics::Xess::Version) == 8, "sizeof(Version) mismatch");
static_assert(sizeof(Libs::Graphics::Xess::ImageViewInfo) == 48, "sizeof(ImageViewInfo) mismatch");
static_assert(sizeof(Libs::Graphics::Xess::InitParams) == 64, "sizeof(InitParams) mismatch");
static_assert(sizeof(Libs::Graphics::Xess::ExecuteParams) == 360, "sizeof(ExecuteParams) mismatch");

static_assert(offsetof(Libs::Graphics::Xess::InitParams, output_resolution) == 0);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, quality_setting) == 8);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, init_flags) == 12);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, creation_node_mask) == 16);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, visible_node_mask) == 20);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, temp_buffer_heap) == 24);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, buffer_heap_offset) == 32);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, temp_texture_heap) == 40);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, texture_heap_offset) == 48);
static_assert(offsetof(Libs::Graphics::Xess::InitParams, pipeline_cache) == 56);

static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, color_texture) == 0);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, velocity_texture) == 48);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, depth_texture) == 96);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, exposure_scale_texture) == 144);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, responsive_pixel_mask_texture) == 192);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, output_texture) == 240);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, jitter_offset_x) == 288);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, jitter_offset_y) == 292);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, exposure_scale) == 296);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, reset_history) == 300);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_width) == 304);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_height) == 308);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_color_base) == 312);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_motion_vector_base) == 320);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_depth_base) == 328);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, input_responsive_mask_base) == 336);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, reserved0) == 344);
static_assert(offsetof(Libs::Graphics::Xess::ExecuteParams, output_color_base) == 352);

void Require(bool value, const char* what) {
	if (value) return;
	std::fprintf(stderr, "kyty_shim_xess: %s\n", what);
	std::abort();
}

void TestEnumParsing() {
	const auto xess_upper = magic_enum::enum_cast<Config::PssrUpscaler>("XESS", magic_enum::case_insensitive);
	Require(xess_upper.has_value() && *xess_upper == Config::PssrUpscaler::XESS, "failed to parse XESS");

	const auto xess_lower = magic_enum::enum_cast<Config::PssrUpscaler>("xess", magic_enum::case_insensitive);
	Require(xess_lower.has_value() && *xess_lower == Config::PssrUpscaler::XESS, "failed to parse xess");

	const auto xess_mixed = magic_enum::enum_cast<Config::PssrUpscaler>("XeSS", magic_enum::case_insensitive);
	Require(xess_mixed.has_value() && *xess_mixed == Config::PssrUpscaler::XESS, "failed to parse XeSS");

	const auto linear = magic_enum::enum_cast<Config::PssrUpscaler>("Linear", magic_enum::case_insensitive);
	Require(linear.has_value() && *linear == Config::PssrUpscaler::Linear, "failed to parse Linear");

	const auto dlss = magic_enum::enum_cast<Config::PssrUpscaler>("DLSS", magic_enum::case_insensitive);
	Require(dlss.has_value() && *dlss == Config::PssrUpscaler::DLSS, "failed to parse DLSS");

	const auto fsr = magic_enum::enum_cast<Config::PssrUpscaler>("FSR", magic_enum::case_insensitive);
	Require(fsr.has_value() && *fsr == Config::PssrUpscaler::FSR, "failed to parse FSR");

	const auto invalid = magic_enum::enum_cast<Config::PssrUpscaler>("UnknownUpscaler", magic_enum::case_insensitive);
	Require(!invalid.has_value(), "invalid upscaler parsed unexpectedly");
}

void TestOptionsFolder() {
	Config::Initialize();
	Config::ConfigOptions options {};
	options.pssr_upscaler = Config::PssrUpscaler::XESS;
	options.xess_folder   = std::filesystem::path("/usr/lib/xess");
	Config::Load(options);

	Require(Config::GetPssrUpscaler() == Config::PssrUpscaler::XESS, "Config::GetPssrUpscaler mismatch");
	Require(Config::GetXessFolder() == std::filesystem::path("/usr/lib/xess"), "Config::GetXessFolder mismatch");
	Config::Shutdown();
}

void TestXeSSConstants() {
	using namespace Libs::Graphics::Xess;

	Require(static_cast<int32_t>(Quality::UltraPerformance) == 100, "Quality::UltraPerformance value mismatch");
	Require(static_cast<int32_t>(Quality::Performance) == 101, "Quality::Performance value mismatch");
	Require(static_cast<int32_t>(Quality::Balanced) == 102, "Quality::Balanced value mismatch");
	Require(static_cast<int32_t>(Quality::Quality) == 103, "Quality::Quality value mismatch");
	Require(static_cast<int32_t>(Quality::UltraQuality) == 104, "Quality::UltraQuality value mismatch");
	Require(static_cast<int32_t>(Quality::UltraQualityPlus) == 105, "Quality::UltraQualityPlus value mismatch");
	Require(static_cast<int32_t>(Quality::NativeAa) == 106, "Quality::NativeAa value mismatch");

	Require(static_cast<int32_t>(NetworkModel::Auto) == 0, "NetworkModel::Auto value mismatch");
	Require(static_cast<int32_t>(NetworkModel::Xmx) == 1, "NetworkModel::Xmx value mismatch");
	Require(static_cast<int32_t>(NetworkModel::Dp4a) == 2, "NetworkModel::Dp4a value mismatch");

	Require(INIT_FLAG_INVERTED_DEPTH == 1u, "INIT_FLAG_INVERTED_DEPTH mismatch");
	Require(INIT_FLAG_ENABLE_AUTOEXPOSURE == 2u, "INIT_FLAG_ENABLE_AUTOEXPOSURE mismatch");
	Require(INIT_FLAG_EXPOSURE_SCALE_TEXTURE == 4u, "INIT_FLAG_EXPOSURE_SCALE_TEXTURE mismatch");
	Require(INIT_FLAG_RESPONSIVE_PIXEL_MASK == 8u, "INIT_FLAG_RESPONSIVE_PIXEL_MASK mismatch");
	Require(RESULT_SUCCESS == 0, "RESULT_SUCCESS mismatch");
}

void TestXeSSChoose() {
	using namespace Libs::Graphics;

	Pssr::Request request {};
	request.width  = 1920;
	request.height = 1080;
	request.flags  = Pssr::kReverseDepth | Pssr::kAutoExposure;

	Dlss::Resource color {.width = 1920, .height = 1080};
	Dlss::Resource depth {.width = 1920, .height = 1080};
	Dlss::Resource motion {.width = 1920, .height = 1080};
	Dlss::Resource output {.width = 3840, .height = 2160};
	Dlss::Inputs inputs {color, depth, motion, nullptr, output};

	const auto chosen = Xess::Choose(request, inputs);
	Require(chosen.has_value(), "Xess::Choose returned nullopt for valid inputs");
	Require(chosen->output_width == 3840, "output_width mismatch");
	Require(chosen->output_height == 2160, "output_height mismatch");
	Require(chosen->max_width == 1920, "max_width mismatch");
	Require(chosen->max_height == 1080, "max_height mismatch");
	Require((chosen->flags & Pssr::kReverseDepth) != 0, "kReverseDepth missing from flags");
	Require((chosen->flags & Pssr::kAutoExposure) != 0, "kAutoExposure missing from flags");
}

void TestXeSSReads() {
	using namespace Libs::Graphics;

	Pssr::Request request {};
	request.x      = 0;
	request.y      = 0;
	request.width  = 1920;
	request.height = 1080;

	Dlss::Resource color {
	    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
	    .width  = 1920,
	    .height = 1080,
	};
	Dlss::Resource depth {
	    .format = VK_FORMAT_D32_SFLOAT,
	    .width  = 1920,
	    .height = 1080,
	};
	Dlss::Resource motion {
	    .format = VK_FORMAT_R16G16_SFLOAT,
	    .width  = 1920,
	    .height = 1080,
	};
	Dlss::Resource output {
	    .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
	    .width  = 3840,
	    .height = 2160,
	};
	Dlss::Inputs inputs {color, depth, motion, nullptr, output};

	Require(Xess::Reads(request, inputs), "Xess::Reads rejected standard float formats");

	request.x = 10;
	Require(!Xess::Reads(request, inputs), "Xess::Reads accepted non-zero subrect x");
	request.x = 0;

	Dlss::Resource integer_color = color;
	integer_color.format = VK_FORMAT_R32G32B32A32_UINT;
	Dlss::Inputs invalid_inputs {integer_color, depth, motion, nullptr, output};
	Require(!Xess::Reads(request, invalid_inputs), "Xess::Reads accepted UINT format");
}

void TestDetermineQuality() {
	using namespace Libs::Graphics::Xess;

	Require(DetermineQuality(1920, 1920) == Quality::NativeAa, "1:1 scaling should select NativeAa");
	Require(DetermineQuality(2953, 3840) == Quality::UltraQualityPlus, "~1.3x scaling should select UltraQualityPlus");
	Require(DetermineQuality(2560, 3840) == Quality::UltraQuality, "1.5x scaling should select UltraQuality");
	Require(DetermineQuality(2258, 3840) == Quality::Quality, "1.7x scaling should select Quality");
	Require(DetermineQuality(1920, 3840) == Quality::Balanced, "2.0x scaling should select Balanced");
	Require(DetermineQuality(1669, 3840) == Quality::Performance, "2.3x scaling should select Performance");
	Require(DetermineQuality(1280, 3840) == Quality::UltraPerformance, "3.0x scaling should select UltraPerformance");
}

void TestXeSSSetCreation() {
	using namespace Libs::Graphics;

	Dlss::Creation creation {
	    .output_width  = 3840,
	    .output_height = 2160,
	    .flags         = Pssr::kReverseDepth | Pssr::kAutoExposure,
	    .max_width     = 1920,
	    .max_height    = 1080,
	};

	Xess::InitParams params {};
	Xess::SetCreation(params, creation);

	Require(params.output_resolution.x == 3840, "output_resolution.x mismatch");
	Require(params.output_resolution.y == 2160, "output_resolution.y mismatch");
	Require(params.quality_setting == Xess::Quality::Balanced, "quality_setting default mismatch");
	Require((params.init_flags & Xess::INIT_FLAG_INVERTED_DEPTH) != 0, "INIT_FLAG_INVERTED_DEPTH missing");
	Require((params.init_flags & Xess::INIT_FLAG_ENABLE_AUTOEXPOSURE) != 0, "INIT_FLAG_ENABLE_AUTOEXPOSURE missing");
	Require(params.creation_node_mask == 0, "creation_node_mask should default to 0");
	Require(params.visible_node_mask == 0, "visible_node_mask should default to 0");
	Require(params.temp_buffer_heap == nullptr, "temp_buffer_heap should default to null");
	Require(params.buffer_heap_offset == 0, "buffer_heap_offset should default to 0");
	Require(params.temp_texture_heap == nullptr, "temp_texture_heap should default to null");
	Require(params.texture_heap_offset == 0, "texture_heap_offset should default to 0");
	Require(params.pipeline_cache == nullptr, "pipeline_cache should default to null");
}

void TestXeSSSetExecute() {
	using namespace Libs::Graphics;

	Pssr::Request request {};
	request.width            = 1920;
	request.height           = 1080;
	request.jitter[0]        = -0.25f;
	request.jitter[1]        = 0.5f;
	request.motion_scale[0]  = 1920.0f;
	request.motion_scale[1]  = 1080.0f;
	request.pre_exposure     = 1.5f;

	Dlss::Resource color {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = 1920, .height = 1080};
	Dlss::Resource depth {
	    .range  = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1},
	    .format = VK_FORMAT_D32_SFLOAT,
	    .width  = 1920,
	    .height = 1080,
	};
	Dlss::Resource motion {.format = VK_FORMAT_R16G16_SFLOAT, .width = 1920, .height = 1080};
	Dlss::Resource exposure {.format = VK_FORMAT_R32_SFLOAT, .width = 1, .height = 1};
	Dlss::Resource output {.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32, .width = 3840, .height = 2160};

	Dlss::Inputs inputs {color, depth, motion, &exposure, output};

	Xess::ExecuteParams params {};
	Xess::SetExecute(params, request, inputs, true);

	Require(params.color_texture.width == 1920 && params.color_texture.height == 1080, "color image dimensions mismatch");
	Require(params.velocity_texture.width == 1920 && params.velocity_texture.height == 1080, "velocity image dimensions mismatch");
	Require(params.depth_texture.subresource_range.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT, "depth stencil aspect was not stripped");
	Require(params.exposure_scale_texture.width == 1 && params.exposure_scale_texture.height == 1, "exposure image dimensions mismatch");
	Require(params.responsive_pixel_mask_texture.width == 0 && params.responsive_pixel_mask_texture.image == nullptr, "responsive_pixel_mask_texture should be empty");
	Require(params.output_texture.width == 3840 && params.output_texture.height == 2160, "output image dimensions mismatch");
	Require(params.jitter_offset_x == -0.25f, "jitter_offset_x mismatch");
	Require(params.jitter_offset_y == 0.5f, "jitter_offset_y mismatch");
	Require(params.input_width == 1920, "input_width mismatch");
	Require(params.input_height == 1080, "input_height mismatch");
	Require(params.exposure_scale == 1.5f, "exposure_scale mismatch");
	Require(params.reset_history == 1u, "reset_history should be 1u");
	Require(params.input_color_base.x == 0 && params.input_color_base.y == 0, "input_color_base mismatch");
	Require(params.input_motion_vector_base.x == 0 && params.input_motion_vector_base.y == 0, "input_motion_vector_base mismatch");
	Require(params.input_depth_base.x == 0 && params.input_depth_base.y == 0, "input_depth_base mismatch");
	Require(params.input_responsive_mask_base.x == 0 && params.input_responsive_mask_base.y == 0, "input_responsive_mask_base mismatch");
	Require(params.reserved0.x == 0 && params.reserved0.y == 0, "reserved0 mismatch");
	Require(params.output_color_base.x == 0 && params.output_color_base.y == 0, "output_color_base mismatch");

	request.x = 8;
	request.y = 16;
	request.pre_exposure = 0.0f;
	Dlss::Inputs inputs_no_exp {color, depth, motion, nullptr, output};
	Xess::ExecuteParams params2 {};
	Xess::SetExecute(params2, request, inputs_no_exp, false);

	Require(params2.exposure_scale_texture.width == 0 && params2.exposure_scale_texture.image == nullptr, "exposure texture should be empty when null");
	Require(params2.exposure_scale == 1.0f, "exposure_scale should default to 1.0f when non-positive");
	Require(params2.reset_history == 0u, "reset_history should be 0u when reset is false");
	Require(params2.input_color_base.x == 8 && params2.input_color_base.y == 16, "input_color_base subrect mismatch");
	Require(params2.input_motion_vector_base.x == 8 && params2.input_motion_vector_base.y == 16, "input_motion_vector_base subrect mismatch");
	Require(params2.input_depth_base.x == 8 && params2.input_depth_base.y == 16, "input_depth_base subrect mismatch");
	Require(params2.input_responsive_mask_base.x == 0 && params2.input_responsive_mask_base.y == 0, "input_responsive_mask_base mismatch");
	Require(params2.reserved0.x == 0 && params2.reserved0.y == 0, "reserved0 mismatch");
	Require(params2.output_color_base.x == 0 && params2.output_color_base.y == 0, "output_color_base mismatch");
}

void TestHardwareModelDetection() {
	using namespace Libs::Graphics::Xess;

	const auto detect = [](uint32_t vendorID) {
		return vendorID == 0x8086 ? NetworkModel::Xmx : NetworkModel::Dp4a;
	};

	Require(detect(0x8086) == NetworkModel::Xmx, "Intel Arc GPU not detected as XMX");
	Require(detect(0x10DE) == NetworkModel::Dp4a, "NVIDIA GPU not detected as DP4a");
	Require(detect(0x1002) == NetworkModel::Dp4a, "AMD GPU not detected as DP4a");
}

void TestAbiMemoryLayout() {
	using namespace Libs::Graphics::Xess;

	Require(sizeof(Coord2D) == 8, "sizeof(Coord2D) mismatch");
	Require(offsetof(Coord2D, x) == 0, "Coord2D::x offset mismatch");
	Require(offsetof(Coord2D, y) == 4, "Coord2D::y offset mismatch");

	Require(sizeof(Version) == 8, "sizeof(Version) mismatch");
	Require(offsetof(Version, major) == 0, "Version::major offset mismatch");
	Require(offsetof(Version, minor) == 2, "Version::minor offset mismatch");
	Require(offsetof(Version, patch) == 4, "Version::patch offset mismatch");
	Require(offsetof(Version, reserved) == 6, "Version::reserved offset mismatch");

	Require(sizeof(ImageViewInfo) == 48, "sizeof(ImageViewInfo) mismatch");
	Require(offsetof(ImageViewInfo, image_view) == 0, "ImageViewInfo::image_view offset mismatch");
	Require(offsetof(ImageViewInfo, image) == 8, "ImageViewInfo::image offset mismatch");
	Require(offsetof(ImageViewInfo, subresource_range) == 16, "ImageViewInfo::subresource_range offset mismatch");
	Require(offsetof(ImageViewInfo, format) == 36, "ImageViewInfo::format offset mismatch");
	Require(offsetof(ImageViewInfo, width) == 40, "ImageViewInfo::width offset mismatch");
	Require(offsetof(ImageViewInfo, height) == 44, "ImageViewInfo::height offset mismatch");

	Require(sizeof(InitParams) == 64, "sizeof(InitParams) mismatch");
	Require(offsetof(InitParams, output_resolution) == 0, "InitParams::output_resolution offset mismatch");
	Require(offsetof(InitParams, quality_setting) == 8, "InitParams::quality_setting offset mismatch");
	Require(offsetof(InitParams, init_flags) == 12, "InitParams::init_flags offset mismatch");
	Require(offsetof(InitParams, creation_node_mask) == 16, "InitParams::creation_node_mask offset mismatch");
	Require(offsetof(InitParams, visible_node_mask) == 20, "InitParams::visible_node_mask offset mismatch");
	Require(offsetof(InitParams, temp_buffer_heap) == 24, "InitParams::temp_buffer_heap offset mismatch");
	Require(offsetof(InitParams, buffer_heap_offset) == 32, "InitParams::buffer_heap_offset offset mismatch");
	Require(offsetof(InitParams, temp_texture_heap) == 40, "InitParams::temp_texture_heap offset mismatch");
	Require(offsetof(InitParams, texture_heap_offset) == 48, "InitParams::texture_heap_offset offset mismatch");
	Require(offsetof(InitParams, pipeline_cache) == 56, "InitParams::pipeline_cache offset mismatch");

	Require(sizeof(ExecuteParams) == 360, "sizeof(ExecuteParams) mismatch");
	Require(offsetof(ExecuteParams, color_texture) == 0, "ExecuteParams::color_texture offset mismatch");
	Require(offsetof(ExecuteParams, velocity_texture) == 48, "ExecuteParams::velocity_texture offset mismatch");
	Require(offsetof(ExecuteParams, depth_texture) == 96, "ExecuteParams::depth_texture offset mismatch");
	Require(offsetof(ExecuteParams, exposure_scale_texture) == 144, "ExecuteParams::exposure_scale_texture offset mismatch");
	Require(offsetof(ExecuteParams, responsive_pixel_mask_texture) == 192, "ExecuteParams::responsive_pixel_mask_texture offset mismatch");
	Require(offsetof(ExecuteParams, output_texture) == 240, "ExecuteParams::output_texture offset mismatch");
	Require(offsetof(ExecuteParams, jitter_offset_x) == 288, "ExecuteParams::jitter_offset_x offset mismatch");
	Require(offsetof(ExecuteParams, jitter_offset_y) == 292, "ExecuteParams::jitter_offset_y offset mismatch");
	Require(offsetof(ExecuteParams, exposure_scale) == 296, "ExecuteParams::exposure_scale offset mismatch");
	Require(offsetof(ExecuteParams, reset_history) == 300, "ExecuteParams::reset_history offset mismatch");
	Require(offsetof(ExecuteParams, input_width) == 304, "ExecuteParams::input_width offset mismatch");
	Require(offsetof(ExecuteParams, input_height) == 308, "ExecuteParams::input_height offset mismatch");
	Require(offsetof(ExecuteParams, input_color_base) == 312, "ExecuteParams::input_color_base offset mismatch");
	Require(offsetof(ExecuteParams, input_motion_vector_base) == 320, "ExecuteParams::input_motion_vector_base offset mismatch");
	Require(offsetof(ExecuteParams, input_depth_base) == 328, "ExecuteParams::input_depth_base offset mismatch");
	Require(offsetof(ExecuteParams, input_responsive_mask_base) == 336, "ExecuteParams::input_responsive_mask_base offset mismatch");
	Require(offsetof(ExecuteParams, reserved0) == 344, "ExecuteParams::reserved0 offset mismatch");
	Require(offsetof(ExecuteParams, output_color_base) == 352, "ExecuteParams::output_color_base offset mismatch");
}

void TestOptInIsolation() {
	using namespace Libs::Graphics;

	Config::Initialize();

	Config::ConfigOptions options {};
	options.pro_enabled   = false;
	options.pssr_upscaler = Config::PssrUpscaler::Linear;
	options.xess_folder   = "";
	Config::Load(options);

	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must be disabled when upscaler is Linear");

	options.pssr_upscaler = Config::PssrUpscaler::DLSS;
	Config::Load(options);
	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must be disabled when upscaler is DLSS");

	options.pssr_upscaler = Config::PssrUpscaler::FSR;
	Config::Load(options);
	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must be disabled when upscaler is FSR");

	options.pssr_upscaler = Config::PssrUpscaler::XESS;
	options.pro_enabled   = false;
	options.xess_folder   = std::filesystem::path("/nonexistent/xess");
	Config::Load(options);
	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must not load when PS5 Pro mode is off");

	options.pro_enabled   = true;
	options.pssr_upscaler = Config::PssrUpscaler::XESS;
	options.xess_folder   = "";
	Config::Load(options);
	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must not load when folder path is empty");

	options.xess_folder   = std::filesystem::path("/tmp/nonexistent_libxess_path");
	Config::Load(options);
	Xess::Load();
	Require(!Xess::Enabled(), "XeSS must stay disabled when runtime binary does not exist");

	Dlss::Creation dummy_creation {};
	Require(Xess::CreateContext(dummy_creation) == nullptr, "CreateContext must return nullptr when disabled");

	Pssr::Request dummy_req {};
	Dlss::Resource res {};
	Dlss::Inputs dummy_inputs {res, res, res, nullptr, res};
	Require(!Xess::Evaluate(vk::CommandBuffer{}, nullptr, dummy_req, dummy_inputs, false), "Evaluate must return false when disabled");

	Xess::ReleaseContext(nullptr);
	Xess::Shutdown();
	Require(!Xess::Enabled(), "XeSS must stay disabled after Shutdown");

	Config::Shutdown();
}

void TestMockAbiDispatch() {
	using namespace Libs::Graphics::Xess;

	struct MockDispatch {
		Result (*get_version)(Version*);
		Result (*destroy_context)(void*);
		Result (*set_velocity_scale)(void*, float, float);
		Result (*set_jitter_scale)(void*, float, float);
		Result (*select_network_model)(void*, int32_t);
		Result (*create_context)(VkInstance, VkPhysicalDevice, VkDevice, void**);
		Result (*init)(void*, const InitParams*);
		Result (*execute)(void*, VkCommandBuffer, const ExecuteParams*);
	} dispatch {
		[](Version* ver) -> Result {
			if (ver == nullptr) return -1;
			ver->major = 2;
			ver->minor = 0;
			ver->patch = 0;
			ver->reserved = 0;
			return RESULT_SUCCESS;
		},
		[](void* ctx) -> Result {
			if (ctx == nullptr) return -1;
			return RESULT_SUCCESS;
		},
		[](void* ctx, float x, float y) -> Result {
			if (ctx == nullptr || x <= 0.0f || y <= 0.0f) return -1;
			return RESULT_SUCCESS;
		},
		[](void* ctx, float x, float y) -> Result {
			if (ctx == nullptr || x <= 0.0f || y <= 0.0f) return -1;
			return RESULT_SUCCESS;
		},
		[](void* ctx, int32_t model) -> Result {
			if (ctx == nullptr || model < 0 || model > 2) return -1;
			return RESULT_SUCCESS;
		},
		[](VkInstance inst, VkPhysicalDevice pd, VkDevice dev, void** ctx) -> Result {
			if (inst == nullptr || pd == nullptr || dev == nullptr || ctx == nullptr) return -1;
			*ctx = reinterpret_cast<void*>(0xcafebeef);
			return RESULT_SUCCESS;
		},
		[](void* ctx, const InitParams* params) -> Result {
			if (ctx == nullptr || params == nullptr) return -1;
			if (params->output_resolution.x == 0 || params->output_resolution.y == 0) return -2;
			return RESULT_SUCCESS;
		},
		[](void* ctx, VkCommandBuffer, const ExecuteParams* params) -> Result {
			if (ctx == nullptr || params == nullptr) return -1;
			if (params->input_width == 0 || params->input_height == 0) return -2;
			return RESULT_SUCCESS;
		},
	};

	Version ver {};
	Require(dispatch.get_version(&ver) == RESULT_SUCCESS && ver.major == 2, "get_version failed");

	void* ctx = nullptr;
	Require(dispatch.create_context(nullptr, reinterpret_cast<VkPhysicalDevice>(0x2), reinterpret_cast<VkDevice>(0x3), &ctx) == -1, "create_context null inst");
	Require(dispatch.create_context(reinterpret_cast<VkInstance>(0x1), nullptr, reinterpret_cast<VkDevice>(0x3), &ctx) == -1, "create_context null pd");
	Require(dispatch.create_context(reinterpret_cast<VkInstance>(0x1), reinterpret_cast<VkPhysicalDevice>(0x2), nullptr, &ctx) == -1, "create_context null dev");
	Require(dispatch.create_context(reinterpret_cast<VkInstance>(0x1), reinterpret_cast<VkPhysicalDevice>(0x2), reinterpret_cast<VkDevice>(0x3), nullptr) == -1, "create_context null ctx");
	Require(dispatch.create_context(reinterpret_cast<VkInstance>(0x1),
	                               reinterpret_cast<VkPhysicalDevice>(0x2),
	                               reinterpret_cast<VkDevice>(0x3),
	                               &ctx) == RESULT_SUCCESS && ctx != nullptr, "create_context failed");
	Require(dispatch.select_network_model(ctx, static_cast<int32_t>(NetworkModel::Xmx)) == RESULT_SUCCESS, "select_network_model failed");
	Require(dispatch.set_velocity_scale(ctx, 1920.0f, 1080.0f) == RESULT_SUCCESS, "set_velocity_scale failed");
	Require(dispatch.set_jitter_scale(ctx, 1.0f, 1.0f) == RESULT_SUCCESS, "set_jitter_scale failed");

	InitParams init_params {};
	init_params.output_resolution = {3840, 2160};
	init_params.quality_setting   = Quality::Balanced;
	init_params.init_flags        = INIT_FLAG_INVERTED_DEPTH | INIT_FLAG_ENABLE_AUTOEXPOSURE;
	Require(dispatch.init(nullptr, &init_params) == -1, "init null context");
	Require(dispatch.init(ctx, nullptr) == -1, "init null params");
	InitParams zero_init {};
	Require(dispatch.init(ctx, &zero_init) == -2, "init zero resolution");
	Require(dispatch.init(ctx, &init_params) == RESULT_SUCCESS, "init failed");

	ExecuteParams exec_params {};
	exec_params.input_width  = 1920;
	exec_params.input_height = 1080;
	Require(dispatch.execute(nullptr, VkCommandBuffer{}, &exec_params) == -1, "execute null context");
	Require(dispatch.execute(ctx, VkCommandBuffer{}, nullptr) == -1, "execute null params");
	ExecuteParams zero_exec {};
	Require(dispatch.execute(ctx, VkCommandBuffer{}, &zero_exec) == -2, "execute zero dimensions");
	Require(dispatch.execute(ctx, VkCommandBuffer{}, &exec_params) == RESULT_SUCCESS, "execute failed");
	Require(dispatch.destroy_context(ctx) == RESULT_SUCCESS, "destroy_context failed");
}

}

int main() {
	TestEnumParsing();
	TestOptionsFolder();
	TestXeSSConstants();
	TestDetermineQuality();
	TestXeSSChoose();
	TestXeSSReads();
	TestXeSSSetCreation();
	TestXeSSSetExecute();
	TestHardwareModelDetection();
	TestAbiMemoryLayout();
	TestOptInIsolation();
	TestMockAbiDispatch();

	std::printf("kyty_shim_xess: ok\n");
	return 0;
}
