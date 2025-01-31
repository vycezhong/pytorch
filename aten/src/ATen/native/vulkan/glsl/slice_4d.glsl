#version 450 core
#define PRECISION $precision

layout(std430) buffer;

/* Qualifiers: layout - storage - precision - memory */

layout(set = 0, binding = 0, rgba16f) uniform PRECISION           image3D uOutput;
layout(set = 0, binding = 1)          uniform PRECISION           sampler3D uInput;
layout(set = 0, binding = 2)          uniform PRECISION restrict  Block {
  ivec4 size;            // output texture size (x=width,y=height,z=depth,w=unused)
  ivec4 isize;           // input texture size (x=width,y=height,z=depth,w=unused)
  uvec4 tensor_size;     // output tensor size
  uvec4 itensor_size;    // input tensor size
  uvec4 args;            // input arguments (dim, start, end, step)
} uBlock;

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

void main() {
  const ivec3 posOut = ivec3(gl_GlobalInvocationID);
  const uint max_dst_index = uBlock.tensor_size[0] * uBlock.tensor_size[1];
  const uint dim = uBlock.args[0];
  const uint start = uBlock.args[1];
  const uint step = uBlock.args[3];

  if (all(lessThan(posOut, uBlock.size.xyz))) {
    vec4 outval = vec4(0.0);
    for (uint j = 0; j < 4; ++j) {
      uint dst_index = posOut.z * 4 + j;
      if (dst_index >= max_dst_index) {
        imageStore(uOutput, posOut, outval);
        // out of range
        break;
      }

      // dst dims
      uint b1 = int(dst_index / uBlock.tensor_size[1]);
      uint c1 = dst_index % uBlock.tensor_size[1];
      uint h1 = posOut.y;
      uint w1 = posOut.x;

      // src dims
      uint b = b1;
      uint c = c1;
      uint h = h1;
      uint w = w1;

      if (dim == 0) { // batch
        b = start + step * b1;
      }
      else if (dim == 1) {  // feature(channel)
        c = start + step * c1;
      }

      uint src_index = b * uBlock.itensor_size[1] + c;
      ivec3 posIn;
      posIn.x = int(w);
      posIn.y = int(h);
      posIn.z = int(src_index / 4);
      uint i = (src_index % 4);

      vec4 inval = texelFetch(uInput, posIn, 0);
      outval[j] = inval[i];

      if (j == 3) {
        imageStore(uOutput, posOut, outval);
      }
    }
  }
}
