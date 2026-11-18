#include "graphics/host_gpu/renderer/upscaler/xess.h"

#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "graphics/host_gpu/graphicContext.h"

#include <SDL3/SDL_loadso.h>
#include <algorithm>
#include <string_view>
#include <vulkan/vulkan_format_traits.hpp>

namespace Libs::Graphics::Xess {

using Dlss::Creation;
using Dlss::Inputs;

namespace {

struct Api {
	Result (*get_version)(Version*);
	Result (*destroy_context)(void*);
	Result (*set_velocity_scale)(void*, float, float);
	Result (*set_jitter_scale)(void*, float, float);
	Result (*select_network_model)(void*, int32_t);
	Result (*create_context)(VkInstance, VkPhysicalDevice, VkDevice, void**);
	Result (*init)(void*, const InitParams*);
	Result (*execute)(void*, VkCommandBuffer, const ExecuteParams*);
};

SDL_SharedObject* g_module          = nullptr;
Api               g_api {};
VkInstance        g_instance        = nullptr;
VkPhysicalDevice  g_physical_device = nullptr;
VkDevice          g_device          = nullptr;
NetworkModel      g_model           = NetworkModel::Auto;

void Warn(std::string_view text) {
	Log::WriteToConsoleAndLog(fmt::format("WARNING: XeSS: {}; PSSR scales linearly.\n", text));
}

template <typename Function>
bool Bind(Function& function, const char* name, const char* alt_name = nullptr) {
	function = reinterpret_cast<Function>(SDL_LoadFunction(g_module, name));
	if (function == nullptr && alt_name != nullptr) {
		function = reinterpret_cast<Function>(SDL_LoadFunction(g_module, alt_name));
	}
	return function != nullptr;
}

ImageViewInfo Describe(const Dlss::Resource* resource) {
	if (resource == nullptr) {
		return {};
	}
	auto range = resource->range;
	if ((range.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) {
		range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	return ImageViewInfo {
	    .image_view        = resource->view,
	    .image             = resource->image,
	    .subresource_range = range,
	    .format            = resource->format,
	    .width             = resource->width,
	    .height            = resource->height,
	};
}

}

bool Reads(const Pssr::Request& request, const Inputs& inputs) {
	const auto known = [](const Dlss::Resource* resource) {
		if (resource == nullptr) {
			return true;
		}
		const std::string_view numeric =
		    vk::componentNumericFormat(static_cast<vk::Format>(resource->format), 0);
		return resource->format != VK_FORMAT_UNDEFINED &&
		       numeric != "UINT" && numeric != "SINT";
	};
	return request.x == 0 && request.y == 0 && known(&inputs.color) && known(&inputs.depth) &&
	       known(&inputs.motion) && known(inputs.exposure) && known(&inputs.output);
}

std::optional<Creation> Choose(const Pssr::Request& request, const Inputs& inputs) {
	const Creation creation {.output_width  = inputs.output.width,
	                         .output_height = inputs.output.height,
	                         .flags         = request.flags & Dlss::FIXED_FLAGS,
	                         .min_width     = 1,
	                         .min_height    = 1,
	                         .max_width     = std::min(inputs.color.width, inputs.output.width),
	                         .max_height    = std::min(inputs.color.height, inputs.output.height)};
	if (!Dlss::Takes(creation, request, inputs)) {
		return std::nullopt;
	}
	return creation;
}

Quality DetermineQuality(uint32_t input_width, uint32_t output_width) {
	if (input_width == 0 || output_width == 0 || input_width >= output_width) {
		return Quality::NativeAa;
	}
	const float scale = static_cast<float>(output_width) / static_cast<float>(input_width);
	if (scale <= 1.4f) {
		return Quality::UltraQualityPlus;
	}
	if (scale <= 1.6f) {
		return Quality::UltraQuality;
	}
	if (scale <= 1.85f) {
		return Quality::Quality;
	}
	if (scale <= 2.15f) {
		return Quality::Balanced;
	}
	if (scale <= 2.65f) {
		return Quality::Performance;
	}
	return Quality::UltraPerformance;
}

void SetCreation(InitParams& params, const Creation& creation) {
	params.output_resolution   = {creation.output_width, creation.output_height};
	params.quality_setting     = DetermineQuality(creation.max_width, creation.output_width);
	params.init_flags          = 0;
	if ((creation.flags & Pssr::kReverseDepth) != 0) {
		params.init_flags |= INIT_FLAG_INVERTED_DEPTH;
	}
	if ((creation.flags & Pssr::kAutoExposure) != 0) {
		params.init_flags |= INIT_FLAG_ENABLE_AUTOEXPOSURE;
	}
	params.creation_node_mask  = 0;
	params.visible_node_mask   = 0;
	params.temp_buffer_heap    = nullptr;
	params.buffer_heap_offset  = 0;
	params.temp_texture_heap   = nullptr;
	params.texture_heap_offset = 0;
	params.pipeline_cache      = nullptr;
}

void SetExecute(ExecuteParams& params, const Pssr::Request& request,
                const Inputs& inputs, bool reset) {
	params.color_texture                 = Describe(&inputs.color);
	params.velocity_texture              = Describe(&inputs.motion);
	params.depth_texture                 = Describe(&inputs.depth);
	params.exposure_scale_texture        = inputs.exposure != nullptr ? Describe(inputs.exposure) : ImageViewInfo {};
	params.responsive_pixel_mask_texture = {};
	params.output_texture                = Describe(&inputs.output);
	params.jitter_offset_x               = request.jitter[0];
	params.jitter_offset_y               = request.jitter[1];
	params.exposure_scale                = request.pre_exposure > 0.0f ? request.pre_exposure : 1.0f;
	params.reset_history                 = reset ? 1u : 0u;
	params.input_width                   = request.width;
	params.input_height                  = request.height;
	params.input_color_base              = {request.x, request.y};
	params.input_motion_vector_base      = {request.x, request.y};
	params.input_depth_base              = {request.x, request.y};
	params.input_responsive_mask_base    = {0, 0};
	params.reserved0                     = {0, 0};
	params.output_color_base             = {0, 0};
}

void Load() {
	if (!Config::Ps5ProEnabled() || Config::GetPssrUpscaler() != Config::PssrUpscaler::XESS) {
		return;
	}
	const auto& folder = Config::GetXessFolder();
	if (folder.empty()) {
		Warn("no folder is set");
		return;
	}
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	g_module = SDL_LoadObject(Common::PathToString(folder / "libxess.dll").c_str());
	if (g_module == nullptr) {
		g_module = SDL_LoadObject(Common::PathToString(folder).c_str());
	}
#else
	g_module = SDL_LoadObject(Common::PathToString(folder / "libxess.so").c_str());
	if (g_module == nullptr) {
		g_module = SDL_LoadObject(Common::PathToString(folder / "libxess.so.1").c_str());
	}
	if (g_module == nullptr) {
		g_module = SDL_LoadObject(Common::PathToString(folder / "libxess.dll").c_str());
	}
	if (g_module == nullptr) {
		g_module = SDL_LoadObject(Common::PathToString(folder).c_str());
	}
#endif
	if (g_module == nullptr) {
		Warn(fmt::format("{} has no libxess library for this system",
		                 Common::PathToString(folder)));
		return;
	}
	Bind(g_api.get_version, "xessGetVersion");
	if (!Bind(g_api.destroy_context, "xessDestroyContext") ||
	    !Bind(g_api.create_context, "xessVKCreateContext", "xessVkCreateContext") ||
	    !Bind(g_api.init, "xessVKInit", "xessVkInit") ||
	    !Bind(g_api.execute, "xessVKExecute", "xessVkExecute")) {
		Warn("the XeSS library lacks required entry points");
		Shutdown();
		return;
	}
	Bind(g_api.set_velocity_scale, "xessSetVelocityScale");
	Bind(g_api.set_jitter_scale, "xessSetJitterScale");
	Bind(g_api.select_network_model, "xessSelectNetworkModel");
}

void Init(const GraphicContext& graphics) {
	if (g_module == nullptr) {
		return;
	}
	g_instance        = static_cast<VkInstance>(graphics.instance);
	g_physical_device = static_cast<VkPhysicalDevice>(graphics.physical_device);
	g_device          = static_cast<VkDevice>(graphics.device);
	const bool is_intel = graphics.physical_device_properties.vendorID == 0x8086;
	g_model = is_intel ? NetworkModel::Xmx : NetworkModel::Dp4a;

	Version version {};
	if (g_api.get_version != nullptr) {
		g_api.get_version(&version);
	}
	Log::WriteToConsoleAndLog(fmt::format(
	    "XeSS: upscaling PSSR with XeSS {}.{}.{} (model: {}) from {}\n",
	    version.major, version.minor, version.patch,
	    is_intel ? "XMX (Intel Arc)" : "DP4a (AMD/NVIDIA)",
	    Common::PathToString(Config::GetXessFolder())));
}

void Shutdown() {
	if (g_module != nullptr) {
		SDL_UnloadObject(g_module);
		g_module = nullptr;
	}
	g_api             = {};
	g_instance        = nullptr;
	g_physical_device = nullptr;
	g_device          = nullptr;
	g_model           = NetworkModel::Auto;
}

bool Enabled() {
	return g_module != nullptr;
}

NetworkModel DetectedModel() {
	return g_model;
}

Context* CreateContext(const Creation& creation) {
	if (g_device == nullptr || g_physical_device == nullptr || g_instance == nullptr ||
	    g_api.create_context == nullptr || g_api.init == nullptr) {
		return nullptr;
	}
	void* context = nullptr;
	if (const auto result = g_api.create_context(g_instance, g_physical_device, g_device, &context);
	    result != RESULT_SUCCESS || context == nullptr) {
		Warn(fmt::format("creating context failed ({})", result));
		return nullptr;
	}
	if (g_api.select_network_model != nullptr && g_model != NetworkModel::Auto) {
		if (const auto result = g_api.select_network_model(context, static_cast<int32_t>(g_model));
		    result != RESULT_SUCCESS) {
			Warn(fmt::format("selecting network model failed ({}), using auto", result));
		}
	}
	InitParams params {};
	SetCreation(params, creation);
	if (const auto result = g_api.init(context, &params); result != RESULT_SUCCESS) {
		Warn(fmt::format("initializing context failed ({})", result));
		g_api.destroy_context(context);
		return nullptr;
	}
	return reinterpret_cast<Context*>(context);
}

bool Evaluate(vk::CommandBuffer command, Context* context, const Pssr::Request& request,
              const Inputs& inputs, bool reset) {
	if (context == nullptr || g_api.execute == nullptr) {
		return false;
	}
	void* handle = reinterpret_cast<void*>(context);
	if (g_api.set_velocity_scale != nullptr) {
		g_api.set_velocity_scale(handle, request.motion_scale[0], request.motion_scale[1]);
	}
	if (g_api.set_jitter_scale != nullptr) {
		g_api.set_jitter_scale(handle, 1.0f, 1.0f);
	}
	ExecuteParams params {};
	SetExecute(params, request, inputs, reset);

	const auto result = g_api.execute(handle, static_cast<VkCommandBuffer>(command), &params);
	if (result != RESULT_SUCCESS) {
		Warn(fmt::format("execution failed ({})", result));
		return false;
	}
	return true;
}

void ReleaseContext(Context* context) {
	if (context != nullptr && g_api.destroy_context != nullptr) {
		g_api.destroy_context(reinterpret_cast<void*>(context));
	}
}

}
