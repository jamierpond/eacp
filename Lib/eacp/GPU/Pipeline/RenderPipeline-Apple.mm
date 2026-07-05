#import <Metal/Metal.h>

#include "RenderPipeline.h"

#include "../Device/Device.h"
#include "../Shader/ShaderLibrary.h"

#include <eacp/Core/ObjC/ObjC.h>

#include <cassert>

namespace eacp::GPU
{
static MTLVertexFormat toMetalVertexFormat(VertexFormat format)
{
    switch (format)
    {
        case VertexFormat::Float:
            return MTLVertexFormatFloat;
        case VertexFormat::Float2:
            return MTLVertexFormatFloat2;
        case VertexFormat::Float3:
            return MTLVertexFormatFloat3;
        case VertexFormat::Float4:
            return MTLVertexFormatFloat4;
    }

    return MTLVertexFormatFloat3;
}

static MTLPixelFormat toMetalPixelFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::BGRA8Unorm:
            return MTLPixelFormatBGRA8Unorm;
    }

    return MTLPixelFormatBGRA8Unorm;
}

static MTLVertexStepFunction toMetalStepFunction(StepRate rate)
{
    switch (rate)
    {
        case StepRate::PerVertex:
            return MTLVertexStepFunctionPerVertex;
        case StepRate::PerInstance:
            return MTLVertexStepFunctionPerInstance;
    }
    return MTLVertexStepFunctionPerVertex;
}

static MTLVertexDescriptor* makeVertexDescriptor(const VertexLayout& layout)
{
    if (layout.attributes.empty())
        return nil;

    auto descriptor = [MTLVertexDescriptor vertexDescriptor];

    for (auto i = 0; i < layout.attributes.size(); ++i)
    {
        const auto& attribute = layout.attributes[i];
        descriptor.attributes[i].format = toMetalVertexFormat(attribute.format);
        descriptor.attributes[i].offset = (NSUInteger) attribute.offset;
        descriptor.attributes[i].bufferIndex = (NSUInteger) attribute.bufferIndex;
    }

    // Multi-slot when `buffers` is populated; single-slot fallback otherwise
    // (pre-instancing shape). Metal needs stride and step function per bound
    // slot; a slot without an entry defaults to PerVertex with stride 0.
    if (! layout.buffers.empty())
    {
        for (auto slot = 0; slot < layout.buffers.size(); ++slot)
        {
            descriptor.layouts[slot].stride = (NSUInteger) layout.buffers[slot].stride;
            descriptor.layouts[slot].stepFunction =
                toMetalStepFunction(layout.buffers[slot].stepRate);
        }
    }
    else
    {
        descriptor.layouts[0].stride = (NSUInteger) layout.stride;
        descriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    }

    return descriptor;
}

// The default guards against a future BlendMode value this backend was never
// taught to handle - it would otherwise silently produce a no-blend pipeline.
// Loud in Debug, degrades to None in Release (both backends match this
// behaviour).
static void applyBlendMode(MTLRenderPipelineColorAttachmentDescriptor* attachment,
                           BlendMode mode)
{
    switch (mode)
    {
        case BlendMode::None:
            break;
        case BlendMode::AlphaBlend:
            attachment.blendingEnabled = YES;
            attachment.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            attachment.destinationRGBBlendFactor =
                MTLBlendFactorOneMinusSourceAlpha;
            attachment.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            attachment.destinationAlphaBlendFactor =
                MTLBlendFactorOneMinusSourceAlpha;
            break;
        case BlendMode::Additive:
            attachment.blendingEnabled = YES;
            attachment.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            attachment.destinationRGBBlendFactor = MTLBlendFactorOne;
            attachment.sourceAlphaBlendFactor = MTLBlendFactorOne;
            attachment.destinationAlphaBlendFactor = MTLBlendFactorOne;
            break;
        default:
            assert(false && "eacp: unhandled BlendMode in Metal backend");
            break;
    }
}

struct RenderPipeline::Native
{
    Native(Device& device, const RenderPipelineDescriptor& descriptor)
        : topology(descriptor.topology)
    {
        auto metalDevice = (__bridge id<MTLDevice>) device.nativeDevice();

        if (metalDevice == nil || descriptor.library == nullptr)
            return;

        auto library = (__bridge id<MTLLibrary>) descriptor.library->nativeLibrary();

        if (library == nil)
            return;

        auto vertexName = @(descriptor.library->vertexEntry().c_str());
        auto fragmentName = @(descriptor.library->fragmentEntry().c_str());

        auto vertexFunction = [library newFunctionWithName:vertexName];
        auto fragmentFunction = [library newFunctionWithName:fragmentName];

        auto pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;

        if (auto vertexDescriptor = makeVertexDescriptor(descriptor.vertexLayout))
            pipelineDescriptor.vertexDescriptor = vertexDescriptor;

        pipelineDescriptor.rasterSampleCount = (NSUInteger) descriptor.sampleCount;

        if (descriptor.depth)
        {
            pipelineDescriptor.depthAttachmentPixelFormat =
                MTLPixelFormatDepth32Float;

            auto depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
            depthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
            depthDescriptor.depthWriteEnabled = YES;
            depthState =
                [metalDevice newDepthStencilStateWithDescriptor:depthDescriptor];
            [depthDescriptor release];
        }

        auto colorAttachment = pipelineDescriptor.colorAttachments[0];
        colorAttachment.pixelFormat = toMetalPixelFormat(descriptor.colorFormat);
        applyBlendMode(colorAttachment, descriptor.blendMode);

        NSError* error = nil;
        state = [metalDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                            error:&error];

        if (state.get() == nil && error != nil)
            LOG(error.localizedDescription.UTF8String);

        [vertexFunction release];
        [fragmentFunction release];
        [pipelineDescriptor release];
    }

    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    ObjC::Ptr<NSObject<MTLRenderPipelineState>> state;
    ObjC::Ptr<NSObject<MTLDepthStencilState>> depthState;
};

RenderPipeline::RenderPipeline(Device& device,
                               const RenderPipelineDescriptor& descriptor)
    : impl(device, descriptor)
{
}

bool RenderPipeline::isValid() const
{
    return impl->state.get() != nil;
}

PrimitiveTopology RenderPipeline::topology() const
{
    return impl->topology;
}

void* RenderPipeline::nativeState() const
{
    return (__bridge void*) impl->state.get();
}

void* RenderPipeline::nativeDepthState() const
{
    return (__bridge void*) impl->depthState.get();
}
} // namespace eacp::GPU
