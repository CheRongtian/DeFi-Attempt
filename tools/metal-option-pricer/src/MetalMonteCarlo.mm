#include "dlp/options/OptionPricing.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#ifndef DLP_OPTION_METALLIB_PATH
#error DLP_OPTION_METALLIB_PATH must point to the compiled Metal shader library
#endif

namespace dlp::options
{

namespace
{

constexpr std::size_t MAX_PATHS = 10'000'000;

struct MetalPricingParameters
{
    float expiryYears;
    float strike;
    float spot;
    float volatility;
    float riskFreeRate;
    std::uint32_t paths;
    std::uint32_t seed;
    std::uint32_t optionType;
};

static_assert(sizeof(MetalPricingParameters) == 32);

std::string NSErrorMessage(NSError* error)
{
    return error == nil ? "unknown Metal error" : std::string{error.localizedDescription.UTF8String};
}

}

class MetalMonteCarlo::Implementation final
{
public:
    Implementation()
    {
        device_ = MTLCreateSystemDefaultDevice();
        if(device_ == nil)
        {
            throw std::runtime_error{"no Metal device is available"};
        }

        queue_ = [device_ newCommandQueue];
        NSError* error = nil;
        NSString* libraryPath = [NSString stringWithUTF8String:DLP_OPTION_METALLIB_PATH];
        NSURL* libraryUrl = [NSURL fileURLWithPath:libraryPath];
        id<MTLLibrary> library = [device_ newLibraryWithURL:libraryUrl error:&error];
        if(library == nil)
        {
            throw std::runtime_error{"failed to load Metal shaders: " + NSErrorMessage(error)};
        }

        id<MTLFunction> function = [library newFunctionWithName:@"priceOptionPaths"];
        pipeline_ = [device_ newComputePipelineStateWithFunction:function error:&error];
        if(pipeline_ == nil)
        {
            throw std::runtime_error{"failed to create Metal pricing pipeline: " + NSErrorMessage(error)};
        }
    }

    [[nodiscard]] PricingResult Price(const PricingRequest& request) const
    {
        const double analyticPrice = BlackScholesPrice(request);
        if(request.paths == 0 || request.paths > MAX_PATHS)
        {
            throw std::invalid_argument{"paths must be between 1 and 10000000"};
        }

        const auto startedAt = std::chrono::steady_clock::now();
        const MetalPricingParameters parameters{
            .expiryYears = static_cast<float>(request.expiryYears),
            .strike = static_cast<float>(request.strike),
            .spot = static_cast<float>(request.spot),
            .volatility = static_cast<float>(request.volatility),
            .riskFreeRate = static_cast<float>(request.riskFreeRate),
            .paths = static_cast<std::uint32_t>(request.paths),
            .seed = request.seed,
            .optionType = request.optionType == OptionType::Call ? 0U : 1U
        };

        id<MTLBuffer> payoffs = [device_
            newBufferWithLength:request.paths * sizeof(float)
            options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> commandBuffer = [queue_ commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setComputePipelineState:pipeline_];
        [encoder setBytes:&parameters length:sizeof(parameters) atIndex:0];
        [encoder setBuffer:payoffs offset:0 atIndex:1];

        const NSUInteger threadsPerGroup = std::min(
            pipeline_.maxTotalThreadsPerThreadgroup,
            pipeline_.threadExecutionWidth * 4U
        );
        [encoder
            dispatchThreads:MTLSizeMake(request.paths, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(threadsPerGroup, 1, 1)];
        [encoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            throw std::runtime_error{
                "Metal pricing command failed: " + NSErrorMessage(commandBuffer.error)
            };
        }

        const auto* values = static_cast<const float*>(payoffs.contents);
        double sum = 0.0;
        double squaredSum = 0.0;
        for(std::size_t index = 0; index < request.paths; ++index)
        {
            const double value = values[index];
            sum += value;
            squaredSum += value * value;
        }

        const double price = sum / static_cast<double>(request.paths);
        const double variance = request.paths > 1
            ? (squaredSum - sum * sum / static_cast<double>(request.paths)) /
                static_cast<double>(request.paths - 1)
            : 0.0;
        const double standardError = std::sqrt(
            std::max(variance, 0.0) / static_cast<double>(request.paths)
        );
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startedAt
        );

        return PricingResult{
            .monteCarloPrice = price,
            .analyticPrice = analyticPrice,
            .standardError = standardError,
            .absoluteDifference = std::abs(price - analyticPrice),
            .paths = request.paths,
            .elapsedMilliseconds = elapsed.count(),
            .device = DeviceName()
        };
    }

    [[nodiscard]] std::string DeviceName() const
    {
        return std::string{device_.name.UTF8String};
    }

private:
    id<MTLDevice> device_;
    id<MTLCommandQueue> queue_;
    id<MTLComputePipelineState> pipeline_;
};

MetalMonteCarlo::MetalMonteCarlo()
    : implementation_(std::make_unique<Implementation>())
{
}

MetalMonteCarlo::~MetalMonteCarlo() = default;
MetalMonteCarlo::MetalMonteCarlo(MetalMonteCarlo&&) noexcept = default;
MetalMonteCarlo& MetalMonteCarlo::operator=(MetalMonteCarlo&&) noexcept = default;

PricingResult MetalMonteCarlo::Price(const PricingRequest& request) const
{
    return implementation_->Price(request);
}

std::string MetalMonteCarlo::DeviceName() const
{
    return implementation_->DeviceName();
}

}
