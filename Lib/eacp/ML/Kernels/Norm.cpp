#include "Norm.h"

#include "../../GPU/Codegen/KernelCache.h"
#include "../../GPU/Frame/ComputePass.h"

namespace eacp::ML
{
using namespace eacp::GPU;

RMSNormKernel::RMSNormKernel()
    : ComputeProgram({normGroupWidth, 1, 1})
{
    compile();
}

void RMSNormKernel::dispatch(ComputePass& pass, int rows, int dim)
{
    dimension = (std::uint32_t) dim;
    pass.dispatch(*this, normGroupWidth, rows);
}

void RMSNormKernel::define()
{
    auto lane = threadPosition().x;
    auto row = threadPosition().y;
    auto base = row * dimension;

    auto sumOfSquares = var(0.f);
    auto col = var(lane);

    loop(col.get() < dimension,
        [&]
        {
            auto value = input[base + col.get()];
            sumOfSquares = sumOfSquares.get() + value * value;
            col = col.get() + (unsigned) normGroupWidth;
        });

    auto meanSquare = groupSum(sumOfSquares.get()) / toFloat(dimension);
    auto scale = rsqrt(meanSquare + epsilon);

    col = lane;

    loop(col.get() < dimension,
        [&]
        {
            auto value = input[base + col.get()];
            write(output, base + col.get(), value * scale * gamma[col.get()]);
            col = col.get() + (unsigned) normGroupWidth;
        });
}

namespace
{
constexpr auto headsPerGroup = normGroupWidth / RMSNormHeadKernel::headWidth;
}

RMSNormHeadKernel::RMSNormHeadKernel()
    : ComputeProgram({normGroupWidth, 1, 1})
{
    compile();
}

void RMSNormHeadKernel::dispatch(ComputePass& pass, int runs)
{
    runCount = (std::uint32_t) runs;
    pass.dispatch(*this, normGroupWidth, (runs + headsPerGroup - 1) / headsPerGroup);
}

void RMSNormHeadKernel::define()
{
    auto lane = threadPosition().x;
    auto slot = lane / (unsigned) headWidth;
    auto element = lane % (unsigned) headWidth;
    auto run = threadPosition().y * (unsigned) headsPerGroup + slot;
    auto inside = run < runCount;
    auto index = min(run, runCount - 1u) * (unsigned) headWidth + element;

    auto value = input[index];
    auto square = select(inside, value * value, 0.f);

    // Two SIMD groups to a run: each folds its half, and the two halves are
    // added in the one add the wide fold makes of them.
    auto halves = shared<Float>((unsigned) normGroupWidth / (unsigned) simdWidth);
    auto half = simdSum(square);

    ifThen(lane % (unsigned) simdWidth == 0u,
           [&] { write(halves, lane / (unsigned) simdWidth, half); });

    barrier();

    auto sum = halves[slot * 2u] + halves[slot * 2u + 1u];
    auto meanSquare = sum / toFloat(unsignedInteger((unsigned) headWidth));
    auto scale = rsqrt(meanSquare + epsilon);

    ifThen(inside, [&] { write(output, index, value * scale * gamma[element]); });
}

LayerNormKernel::LayerNormKernel()
    : ComputeProgram({normGroupWidth, 1, 1})
{
    compile();
}

void LayerNormKernel::dispatch(ComputePass& pass, int rows, int dim)
{
    dimension = (std::uint32_t) dim;
    pass.dispatch(*this, normGroupWidth, rows);
}

void LayerNormKernel::define()
{
    auto lane = threadPosition().x;
    auto row = threadPosition().y;
    auto base = row * dimension;

    auto sum = var(0.f);
    auto col = var(lane);

    loop(col.get() < dimension,
        [&]
        {
            sum = sum.get() + input[base + col.get()];
            col = col.get() + (unsigned) normGroupWidth;
        });

    auto mean = groupSum(sum.get()) / toFloat(dimension);

    auto sumSquaredDeviation = var(0.f);
    col = lane;

    loop(col.get() < dimension,
        [&]
        {
            auto centred = input[base + col.get()] - mean;
            sumSquaredDeviation = sumSquaredDeviation.get() + centred * centred;
            col = col.get() + (unsigned) normGroupWidth;
        });

    auto variance = groupSum(sumSquaredDeviation.get()) / toFloat(dimension);
    auto scale = rsqrt(variance + epsilon);

    col = lane;

    loop(col.get() < dimension,
        [&]
        {
            auto centred = input[base + col.get()] - mean;

            write(output,
                 base + col.get(),
                 centred * scale * gamma[col.get()] + beta[col.get()]);

            col = col.get() + (unsigned) normGroupWidth;
        });
}

LayerNormNoBiasKernel::LayerNormNoBiasKernel()
    : ComputeProgram({normGroupWidth, 1, 1})
{
    compile();
}

void LayerNormNoBiasKernel::dispatch(ComputePass& pass, int rows, int dim)
{
    dimension = (std::uint32_t) dim;
    pass.dispatch(*this, normGroupWidth, rows);
}

void LayerNormNoBiasKernel::define()
{
    auto lane = threadPosition().x;
    auto row = threadPosition().y;
    auto base = row * dimension;

    auto sum = var(0.f);
    auto col = var(lane);

    loop(col.get() < dimension,
        [&]
        {
            sum = sum.get() + input[base + col.get()];
            col = col.get() + (unsigned) normGroupWidth;
        });

    auto mean = groupSum(sum.get()) / toFloat(dimension);

    auto sumSquaredDeviation = var(0.f);
    col = lane;

    loop(col.get() < dimension,
        [&]
        {
            auto centred = input[base + col.get()] - mean;
            sumSquaredDeviation = sumSquaredDeviation.get() + centred * centred;
            col = col.get() + (unsigned) normGroupWidth;
        });

    auto variance = groupSum(sumSquaredDeviation.get()) / toFloat(dimension);
    auto scale = rsqrt(variance + epsilon);

    col = lane;

    loop(col.get() < dimension,
        [&]
        {
            auto centred = input[base + col.get()] - mean;
            write(output, base + col.get(), centred * scale * gamma[col.get()]);
            col = col.get() + (unsigned) normGroupWidth;
        });
}

DynamicTanhKernel::DynamicTanhKernel()
{
    compile();
}

void DynamicTanhKernel::dispatch(ComputePass& pass, int rows, int dim)
{
    dimension = (std::uint32_t) dim;
    pass.dispatch(*this, dim, rows);
}

void DynamicTanhKernel::define()
{
    auto position = threadPosition();
    auto index = position.y * dimension + position.x;

    auto value =
        saturatingTanh(alpha * input[index]) * gamma[position.x] + beta[position.x];
    write(output, index, value);
}

Tensor rmsNorm(ComputePass& pass,
              const Tensor& input,
              const Tensor& gamma,
              float epsilon,
              Device& device)
{
    auto rows = input.rows();
    auto dim = input.cols();
    auto result = Tensor::uninitializedF32({rows, dim}, device);

    auto& kernel = sharedKernel<RMSNormKernel>(device);
    kernel.input = input.buffer();
    kernel.gamma = gamma.buffer();
    kernel.output = result.buffer();
    kernel.epsilon = epsilon;
    kernel.dispatch(pass, rows, dim);

    return result;
}

Tensor layerNorm(ComputePass& pass,
                 const Tensor& input,
                 const Tensor& gamma,
                 const Tensor* beta,
                 float epsilon,
                 Device& device)
{
    auto rows = input.rows();
    auto dim = input.cols();
    auto result = Tensor::uninitializedF32({rows, dim}, device);

    if (beta != nullptr)
    {
        auto& kernel = sharedKernel<LayerNormKernel>(device);
        kernel.input = input.buffer();
        kernel.gamma = gamma.buffer();
        kernel.beta = beta->buffer();
        kernel.output = result.buffer();
        kernel.epsilon = epsilon;
        kernel.dispatch(pass, rows, dim);
    }
    else
    {
        auto& kernel = sharedKernel<LayerNormNoBiasKernel>(device);
        kernel.input = input.buffer();
        kernel.gamma = gamma.buffer();
        kernel.output = result.buffer();
        kernel.epsilon = epsilon;
        kernel.dispatch(pass, rows, dim);
    }

    return result;
}

Tensor dynamicTanh(ComputePass& pass,
                   const Tensor& input,
                   const Tensor& gamma,
                   const Tensor& beta,
                   float alpha,
                   Device& device)
{
    auto rows = input.rows();
    auto dim = input.cols();
    auto result = Tensor::uninitializedF32({rows, dim}, device);

    auto& kernel = sharedKernel<DynamicTanhKernel>(device);
    kernel.input = input.buffer();
    kernel.gamma = gamma.buffer();
    kernel.beta = beta.buffer();
    kernel.output = result.buffer();
    kernel.alpha = alpha;
    kernel.dispatch(pass, rows, dim);

    return result;
}

Tensor rmsNormPerHead(ComputePass& pass,
                      const Tensor& input,
                      const Tensor& gamma,
                      int headDim,
                      float epsilon,
                      Device& device)
{
    auto result = Tensor::uninitializedF32(input.shape(), device);

    if (headDim == RMSNormHeadKernel::headWidth)
    {
        auto& kernel = sharedKernel<RMSNormHeadKernel>(device);
        kernel.input = input.buffer();
        kernel.gamma = gamma.buffer();
        kernel.output = result.buffer();
        kernel.epsilon = epsilon;
        kernel.dispatch(pass, input.count() / headDim);

        return result;
    }

    auto& kernel = sharedKernel<RMSNormKernel>(device);
    kernel.input = input.buffer();
    kernel.gamma = gamma.buffer();
    kernel.output = result.buffer();
    kernel.epsilon = epsilon;
    kernel.dispatch(pass, input.count() / headDim, headDim);

    return result;
}

Tensor dynamicTanhPerHead(ComputePass& pass,
                          const Tensor& input,
                          const Tensor& gamma,
                          const Tensor& beta,
                          float alpha,
                          int headDim,
                          Device& device)
{
    auto result = Tensor::uninitializedF32(input.shape(), device);

    auto& kernel = sharedKernel<DynamicTanhKernel>(device);
    kernel.input = input.buffer();
    kernel.gamma = gamma.buffer();
    kernel.beta = beta.buffer();
    kernel.output = result.buffer();
    kernel.alpha = alpha;
    kernel.dispatch(pass, input.count() / headDim, headDim);

    return result;
}
}
