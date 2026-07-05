#include "ShaderBuilder.h"

#include "../Pipeline/VertexLayout.h"

#include <cassert>
#include <utility>

namespace eacp::GPU
{
namespace
{
// Matrix and uint values are never vertex attributes, so they fall back to
// Float4.
VertexFormat toVertexFormat(ValueType type)
{
    switch (type)
    {
        case ValueType::Float:
            return VertexFormat::Float;
        case ValueType::Float2:
            return VertexFormat::Float2;
        case ValueType::Float3:
            return VertexFormat::Float3;
        case ValueType::Float4:
        case ValueType::Float4x4:
        case ValueType::UInt:
            return VertexFormat::Float4;
    }

    return VertexFormat::Float;
}

// Groups inputs by slot: each slot's attributes accumulate their offsets in
// declaration order, and the slot's stride is the sum of its byte sizes.
// Step rate is inherited from the first attribute assigned to a slot -
// callers who pass explicit bufferIndex are responsible for keeping all
// attributes in a given slot at the same rate. Mixing PerVertex +
// PerInstance in a single slot would produce a subtly wrong pipeline that
// each backend resolves differently - a silent cross-platform footgun -
// so Debug asserts loudly, matching the assert-on-unhandled-mode
// convention added in the BlendModes PR.
//
// With instancing, the layout publishes stride + rate for every slot the
// graph populated; empty leading slots (rare) default to PerVertex with
// stride 0, a safe no-op for backends. Without instancing, the
// single-buffer shape (buffers empty, stride populated) is kept so
// existing single-buffer consumers see no change.
VertexLayout buildVertexLayout(const ShaderGraph& graph)
{
    auto layout = VertexLayout {};

    auto perSlotOffsets = Vector<int> {};
    auto perSlotRates = Vector<StepRate> {};
    auto sawInstance = false;

    for (auto i = 0; i < graph.inputs().size(); ++i)
    {
        auto type = graph.inputs()[i];
        auto rate = graph.inputStepRates()[i];
        auto slot = graph.inputBufferIndices()[i];

        while (perSlotOffsets.size() <= slot)
        {
            perSlotOffsets.add(0);
            perSlotRates.add(StepRate::PerVertex);
        }

        auto firstInSlot = perSlotOffsets[slot] == 0;
        if (firstInSlot)
        {
            perSlotRates[slot] = rate;
        }
        else
        {
            assert(perSlotRates[slot] == rate
                   && "eacp: attributes in a single vertex-buffer slot must "
                      "share a step rate (all PerVertex or all PerInstance)");
        }

        layout.attribute(toVertexFormat(type), perSlotOffsets[slot], slot);
        perSlotOffsets[slot] += byteSize(type);

        if (rate == StepRate::PerInstance)
            sawInstance = true;
    }

    if (sawInstance)
    {
        for (auto slot = 0; slot < perSlotOffsets.size(); ++slot)
            layout.buffer(slot, perSlotOffsets[slot], perSlotRates[slot]);
    }
    else
    {
        layout.stride = perSlotOffsets.empty() ? 0 : perSlotOffsets[0];
    }

    return layout;
}
} // namespace

void ShaderBuilder::position(const Float4& clipPosition)
{
    graphData.setPosition(clipPosition.node);
}

void ShaderBuilder::fragment(const Float4& color)
{
    graphData.setFragment(color.node);
}

GeneratedShader ShaderBuilder::build() const
{
    auto source = detail::nativeShaderSource(graphData);

    auto result = GeneratedShader {};

    if (graphData.isCompute())
    {
        source.withCompute("computeMain");
        result.source = std::move(source);
        return result;
    }

    source.withVertex("vertexMain").withFragment("fragmentMain");
    result.source = std::move(source);
    result.vertexLayout = buildVertexLayout(graphData);
    return result;
}
} // namespace eacp::GPU
