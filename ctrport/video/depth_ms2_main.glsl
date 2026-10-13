// SPDX-License-Identifier: GPL-2.0-only
// Dispatch adapter only; preceding public tiling.comp address functions stay intact.
void main() {
    uint output_index = gl_GlobalInvocationID.x * NUM_SAMPLES + gl_GlobalInvocationID.y;
    uint texel = output_index;
    uint mip = GetMipLevel(texel);
    uint pitch = info.mips[mip].pitch;
    uint height = info.mips[mip].height;
    uint pixel = texel / NUM_SAMPLES;
    uint x = pixel % pitch;
    uint y = (pixel / pitch) % height;
    uint slice = pixel / (pitch * height);
    uint tiled_offset = info.mips[mip].offset + ComputeSurfaceAddrFromCoordMacroTiled(x, y, slice, pitch, height, texel % NUM_SAMPLES);
#ifdef IS_TILER
    tiled_data[tiled_offset / BYTES_PER_PIXEL] = linear_data[output_index];
#else
    linear_data[output_index] = tiled_data[tiled_offset / BYTES_PER_PIXEL];
#endif
}
