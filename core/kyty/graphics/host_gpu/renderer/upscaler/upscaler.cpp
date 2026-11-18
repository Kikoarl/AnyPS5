#include "graphics/host_gpu/renderer/upscaler/upscaler.h"

#include "graphics/host_gpu/renderer/upscaler/fsr.h"
#include "graphics/host_gpu/renderer/upscaler/xess.h"

namespace Libs::Graphics::Upscaler {
namespace {

class DlssFeature final: public Feature {
public:
	explicit DlssFeature(Dlss::Feature* feature): m_feature(feature) {}
	~DlssFeature() override { Dlss::ReleaseFeature(m_feature); }

	bool Evaluate(vk::CommandBuffer command, const Pssr::Request& request, Dlss::Inputs& inputs,
	              bool reset) override {
		return Dlss::Evaluate(command, m_feature, request, inputs, reset);
	}

private:
	Dlss::Feature* m_feature;
};

class FsrFeature final: public Feature {
public:
	explicit FsrFeature(Fsr::Context* context): m_context(context) {}
	~FsrFeature() override { Fsr::ReleaseContext(m_context); }

	bool Evaluate(vk::CommandBuffer command, const Pssr::Request& request, Dlss::Inputs& inputs,
	              bool reset) override {
		return Fsr::Evaluate(command, m_context, request, inputs, reset);
	}

private:
	Fsr::Context* m_context;
};

class XessFeature final: public Feature {
public:
	explicit XessFeature(Xess::Context* context): m_context(context) {}
	~XessFeature() override { Xess::ReleaseContext(m_context); }

	bool Evaluate(vk::CommandBuffer command, const Pssr::Request& request, Dlss::Inputs& inputs,
	              bool reset) override {
		return Xess::Evaluate(command, m_context, request, inputs, reset);
	}

private:
	Xess::Context* m_context;
};

}

bool Enabled() {
	return Dlss::Capabilities() != nullptr || Fsr::Enabled() || Xess::Enabled();
}

std::optional<Dlss::Creation> Choose(const Pssr::Request& request, const Dlss::Inputs& inputs) {
	if (Xess::Enabled()) {
		return Xess::Choose(request, inputs);
	}
	return Fsr::Enabled() ? Fsr::Choose(request, inputs)
	                      : Dlss::Choose(*Dlss::Capabilities(), request, inputs);
}

std::unique_ptr<Feature> Create(vk::CommandBuffer command, const Dlss::Creation& creation) {
	if (Xess::Enabled()) {
		auto* context = Xess::CreateContext(creation);
		return context != nullptr ? std::make_unique<XessFeature>(context) : nullptr;
	}
	if (Fsr::Enabled()) {
		auto* context = Fsr::CreateContext(creation);
		return context != nullptr ? std::make_unique<FsrFeature>(context) : nullptr;
	}
	auto* feature = Dlss::CreateFeature(command, creation);
	return feature != nullptr ? std::make_unique<DlssFeature>(feature) : nullptr;
}

}
