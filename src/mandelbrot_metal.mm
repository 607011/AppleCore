#include <atomic>
#include <cmath>
#include <complex>
#include <iostream>
#include <mutex>
#include <sys/types.h>
#include <utility>
#include <vector>

#include <Metal/Metal.h>
#include <simd/simd.h>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

#include "util.hpp"

#include "mandelbrot_metal.h"

namespace
{
using iteration_count_t = uint64_t;

struct mandelbrot_calculator_metal
{
    static const char* const mandelbrot_shader = R"(
        kernel void mandelbrot_kernel(
            device unsigned long long *outputBuffer [[buffer(0)]],
            constant double2 &resolution [[buffer(1)]],
            constant double &scale [[buffer(2)]],
            constant unsigned long long& max_iterations [[buffer(3)]],
            uint2 gid [[thread_position_in_grid]]
        )
        {
        double2 normalizedCoords = double2(gid) / resolution;
        double2 c = (normalizedCoords - 0.5) * 2.0 - double2(0.75, 0.0);
        double2 z = {0.0, 0.0};
        unsigned long long iterations = 0;
        while (iterations < max_iterations && dot_product(z, z) < 4.0f) {
            z = double2(z.x * z.x - z.y * z.y, 2.0f * z.x * z.y) + c;
            iterations++;
        }
        outputBuffer[gid.y * width + gid.x] = iterations;
    }
    )";

    iteration_count_t base_iterations = 100;
    double log_scale_factor = 0.25;
    iteration_count_t max_iterations_limit = 2'000'000'000ULL;
    std::atomic<int> completed_rows = 0;
    int width = 3840;
    int height = 2160;
    std::mutex output_mtx;

    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLComputePipelineState> _computePipelineState;

    mandelbrot_calculator_metal() {
        _device = MTLCreateSystemDefaultDevice();
        _commandQueue = [_device newCommandQueue];
        
        // Compile Metal shader
        NSError* error = nil;
        id<MTLLibrary> library = [_device newLibraryWithSource:@(mandelbrot_shader)
                                                       options:nil
                                                         error:&error];
        
        id<MTLFunction> kernelFunction = [library newFunctionWithName:@"mandelbrot_kernel"];
        _computePipelineState = [_device newComputePipelineStateWithFunction:kernelFunction error:&error];
    }

    void render(
        int width,
        int height,
        simd_double2 center,
        double zoom,
        iteration_count_t max_iterations
    ) {
        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        
        // Create compute encoder
        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
        [computeEncoder setComputePipelineState:_computePipelineState];
        
        // Create output texture
        MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                                                                     width:width
                                                                                                    height:height
                                                                                                 mipmapped:NO];
        id<MTLTexture> outputTexture = [_device newTextureWithDescriptor:textureDescriptor];
        
        // Set kernel parameters
        [computeEncoder setTexture:outputTexture atIndex:0];
        [computeEncoder setBytes:&center length:sizeof(simd_float2) atIndex:0];
        [computeEncoder setBytes:&zoom length:sizeof(float) atIndex:1];
        [computeEncoder setBytes:&max_iterations length:sizeof(int) atIndex:2];
        
        // Dispatch compute kernel
        MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
        MTLSize threadGroups = MTLSizeMake(
            (width + threadGroupSize.width - 1) / threadGroupSize.width,
            (height + threadGroupSize.height - 1) / threadGroupSize.height,
            1
        );
        [computeEncoder dispatchThreadgroups:threadGroups threadsPerThreadgroup:threadGroupSize];
        [computeEncoder endEncoding];
        
        // Commit command buffer
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
    }

    void reset(void)
    {
        completed_rows = 0;
    }

    inline iteration_count_t calculate(FloatType const& x0, FloatType const& y0, const iteration_count_t max_iterations)
    {
        
    }

    void calculate_mandelbrot_row(work_item<double> const& w)
    {
        ++completed_rows;
    }

    iteration_count_t calculate_max_iterations(double zoom_level)
    {
        iteration_count_t max_iterations =
            static_cast<iteration_count_t>(base_iterations * std::exp(log_scale_factor * zoom_level));
        return max_iterations;
    }
};

} // namespace
