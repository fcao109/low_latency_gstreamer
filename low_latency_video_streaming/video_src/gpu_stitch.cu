// ---------------------------------------------------------------------------
// GPU-accelerated YUV420 frame stitching and cropping.
//
// These kernels replace the CPU memcpy loops in stitchFrames() and
// cropFrames() (utils.cpp).  On Jetson's unified memory the main benefit is
// freeing the CPU core while the GPU handles the data movement; on a discrete
// GPU the benefit is that source and destination can both live in device memory
// without round-tripping through the CPU.
//
// All pointers are expected to be accessible by the GPU:
//   - cudaMallocHost (Jetson zero-copy / pinned host)
//   - cudaMalloc     (device memory, GPUDirect path)
// ---------------------------------------------------------------------------

#include <cuda_runtime.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Vertical stitch (horizontal3D == 0):
//   Y:  [frame1_Y ]    →  combined Y  = [frame1_Y; frame2_Y]
//       [frame2_Y ]
//   U:  [frame1_U ]    →  combined U  = [frame1_U; frame2_U]
//       [frame2_U ]
//   V:  ditto
//
// Each thread handles one byte.
// ---------------------------------------------------------------------------
__global__ void stitchVerticalYUV420Kernel(
    const unsigned char* __restrict__ frame1,
    const unsigned char* __restrict__ frame2,
    unsigned char* __restrict__ dst,
    int width, int height)
{
    int y_size   = width * height;
    int uv_size  = (width / 2) * (height / 2);
    int total    = 2 * y_size + 2 * 2 * uv_size;  // combined frame size

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total) return;

    // Y plane: first y_size bytes from frame1, next y_size from frame2
    if (idx < 2 * y_size) {
        if (idx < y_size)
            dst[idx] = frame1[idx];
        else
            dst[idx] = frame2[idx - y_size];
        return;
    }

    // U plane: next uv_size from frame1, then uv_size from frame2
    int u_base = 2 * y_size;
    if (idx < u_base + 2 * uv_size) {
        int u_idx = idx - u_base;
        if (u_idx < uv_size)
            dst[idx] = frame1[y_size + u_idx];
        else
            dst[idx] = frame2[y_size + (u_idx - uv_size)];
        return;
    }

    // V plane: next uv_size from frame1, then uv_size from frame2
    int v_base = u_base + 2 * uv_size;
    int v_idx = idx - v_base;
    if (v_idx < uv_size)
        dst[idx] = frame1[y_size + uv_size + v_idx];
    else
        dst[idx] = frame2[y_size + uv_size + (v_idx - uv_size)];
}

// ---------------------------------------------------------------------------
// Horizontal stitch (horizontal3D == 1):
//   Y rows are interleaved: [frame1_row | frame2_row] for each row.
//   U/V rows are similarly interleaved at half resolution.
// ---------------------------------------------------------------------------
__global__ void stitchHorizontalYUV420Kernel(
    const unsigned char* __restrict__ frame1,
    const unsigned char* __restrict__ frame2,
    unsigned char* __restrict__ dst,
    int width, int height)
{
    int combined_width = width * 2;
    int y_size  = width * height;
    int uv_w    = width / 2;
    int uv_h    = height / 2;
    int uv_size = uv_w * uv_h;

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = combined_width * height + (combined_width / 2) * uv_h * 2;
    if (idx >= total) return;

    // Y plane
    int y_total = combined_width * height;
    if (idx < y_total) {
        int row = idx / combined_width;
        int col = idx % combined_width;
        if (col < width)
            dst[idx] = frame1[row * width + col];
        else
            dst[idx] = frame2[row * width + (col - width)];
        return;
    }

    // U plane
    int cu_w = combined_width / 2;
    int uv_total = cu_w * uv_h;
    int u_base = y_total;
    if (idx < u_base + uv_total) {
        int ui = idx - u_base;
        int row = ui / cu_w;
        int col = ui % cu_w;
        if (col < uv_w)
            dst[idx] = frame1[y_size + row * uv_w + col];
        else
            dst[idx] = frame2[y_size + row * uv_w + (col - uv_w)];
        return;
    }

    // V plane
    int v_base = u_base + uv_total;
    int vi = idx - v_base;
    int row = vi / cu_w;
    int col = vi % cu_w;
    if (col < uv_w)
        dst[idx] = frame1[y_size + uv_size + row * uv_w + col];
    else
        dst[idx] = frame2[y_size + uv_size + row * uv_w + (col - uv_w)];
}

// ---------------------------------------------------------------------------
// Crop kernel for YUV420 planar (I420).
// Each thread copies one byte from the appropriate plane.
// ---------------------------------------------------------------------------
__global__ void cropYUV420Kernel(
    const unsigned char* __restrict__ src,
    unsigned char* __restrict__ dst,
    int srcWidth, int srcHeight,
    int cropX, int cropY,
    int cropWidth, int cropHeight)
{
    int y_crop_size  = cropWidth * cropHeight;
    int uv_crop_w    = cropWidth / 2;
    int uv_crop_h    = cropHeight / 2;
    int uv_crop_size = uv_crop_w * uv_crop_h;
    int total = y_crop_size + 2 * uv_crop_size;

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total) return;

    int src_y_size  = srcWidth * srcHeight;
    int src_uv_w    = srcWidth / 2;
    int src_uv_size = src_uv_w * (srcHeight / 2);

    // Y plane
    if (idx < y_crop_size) {
        int r = idx / cropWidth;
        int c = idx % cropWidth;
        dst[idx] = src[(cropY + r) * srcWidth + cropX + c];
        return;
    }

    // U plane
    if (idx < y_crop_size + uv_crop_size) {
        int ui = idx - y_crop_size;
        int r = ui / uv_crop_w;
        int c = ui % uv_crop_w;
        int src_off = src_y_size + (cropY / 2 + r) * src_uv_w + (cropX / 2 + c);
        dst[idx] = src[src_off];
        return;
    }

    // V plane
    int vi = idx - y_crop_size - uv_crop_size;
    int r = vi / uv_crop_w;
    int c = vi % uv_crop_w;
    int src_off = src_y_size + src_uv_size + (cropY / 2 + r) * src_uv_w + (cropX / 2 + c);
    dst[idx] = src[src_off];
}

// ---------------------------------------------------------------------------
// Host-callable wrappers.  These are declared extern "C" so they can be called
// from plain C++ translation units without name mangling.
// ---------------------------------------------------------------------------

static cudaStream_t g_stitch_stream = nullptr;

extern "C" void gpu_stitch_init() {
    if (!g_stitch_stream) {
        cudaStreamCreateWithFlags(&g_stitch_stream, cudaStreamNonBlocking);
    }
}

extern "C" void gpu_stitch_cleanup() {
    if (g_stitch_stream) {
        cudaStreamDestroy(g_stitch_stream);
        g_stitch_stream = nullptr;
    }
}

extern "C" void gpu_stitch_vertical_yuv420(
    const unsigned char* frame1,
    const unsigned char* frame2,
    unsigned char* dst,
    int width, int height)
{
    int y_size  = width * height;
    int uv_size = (width / 2) * (height / 2);
    int total   = 2 * y_size + 4 * uv_size;

    int threads = 256;
    int blocks  = (total + threads - 1) / threads;

    stitchVerticalYUV420Kernel<<<blocks, threads, 0, g_stitch_stream>>>(
        frame1, frame2, dst, width, height);
    cudaStreamSynchronize(g_stitch_stream);
}

extern "C" void gpu_stitch_horizontal_yuv420(
    const unsigned char* frame1,
    const unsigned char* frame2,
    unsigned char* dst,
    int width, int height)
{
    int combined_width = width * 2;
    int total = combined_width * height + (combined_width / 2) * (height / 2) * 2;

    int threads = 256;
    int blocks  = (total + threads - 1) / threads;

    stitchHorizontalYUV420Kernel<<<blocks, threads, 0, g_stitch_stream>>>(
        frame1, frame2, dst, width, height);
    cudaStreamSynchronize(g_stitch_stream);
}

extern "C" void gpu_crop_yuv420(
    const unsigned char* src,
    unsigned char* dst,
    int srcWidth, int srcHeight,
    int cropX, int cropY,
    int cropWidth, int cropHeight)
{
    int total = cropWidth * cropHeight + 2 * (cropWidth / 2) * (cropHeight / 2);

    int threads = 256;
    int blocks  = (total + threads - 1) / threads;

    cropYUV420Kernel<<<blocks, threads, 0, g_stitch_stream>>>(
        src, dst, srcWidth, srcHeight, cropX, cropY, cropWidth, cropHeight);
    cudaStreamSynchronize(g_stitch_stream);
}

// ---------------------------------------------------------------------------
// Pinned-memory allocator for the canvas buffer pool.  Returns an array of
// num_images pointers, each pointing to a cudaMallocHost'd buffer.
// ---------------------------------------------------------------------------
extern "C" unsigned char** gpu_allocate_pinned_images(
    int width, int height, int num_images, int is_yuv420)
{
    int image_size = is_yuv420 ? (width * height * 3 / 2)
                               : (width * height * 3);

    unsigned char **images = (unsigned char**)malloc(num_images * sizeof(unsigned char*));
    if (!images) {
        fprintf(stderr, "gpu_allocate_pinned_images: pointer array alloc failed\n");
        return nullptr;
    }
    for (int i = 0; i < num_images; i++) {
        cudaError_t err = cudaMallocHost((void**)&images[i], image_size);
        if (err != cudaSuccess || !images[i]) {
            fprintf(stderr, "cudaMallocHost failed for image %d: %s\n",
                    i, cudaGetErrorString(err));
            for (int j = 0; j < i; j++) cudaFreeHost(images[j]);
            free(images);
            return nullptr;
        }
    }
    return images;
}

extern "C" void gpu_free_pinned_images(unsigned char** images, int num_images) {
    if (!images) return;
    for (int i = 0; i < num_images; i++) {
        if (images[i]) cudaFreeHost(images[i]);
    }
    free(images);
}
