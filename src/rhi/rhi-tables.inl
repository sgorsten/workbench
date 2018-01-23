// This file is designed to ease the pain of implementing various features and modes in a cross-API fashion. For instance:
//   switch(value)
//   {
//   #define RHI_VALUE(VALUE, VK, DX, GL) case VALUE: ...; break;
//   #include "rhi-tables.inl"
//   default: fail_fast();
//   }

// #define RHI_SHADER_STAGE(CASE, VK, GL)
#ifdef RHI_SHADER_STAGE
RHI_SHADER_STAGE(rhi::shader_stage::vertex,                 VK_SHADER_STAGE_VERTEX_BIT,                  GL_VERTEX_SHADER)
RHI_SHADER_STAGE(rhi::shader_stage::tesselation_control,    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    GL_TESS_CONTROL_SHADER)
RHI_SHADER_STAGE(rhi::shader_stage::tesselation_evaluation, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, GL_TESS_EVALUATION_SHADER)
RHI_SHADER_STAGE(rhi::shader_stage::geometry,               VK_SHADER_STAGE_GEOMETRY_BIT,                GL_GEOMETRY_SHADER)
RHI_SHADER_STAGE(rhi::shader_stage::fragment,               VK_SHADER_STAGE_FRAGMENT_BIT,                GL_FRAGMENT_SHADER)
RHI_SHADER_STAGE(rhi::shader_stage::compute,                VK_SHADER_STAGE_COMPUTE_BIT,                 GL_COMPUTE_SHADER)
#undef RHI_SHADER_STAGE
#endif

// #define RHI_ADDRESS_MODE(CASE, VK, DX, GL)
#ifdef RHI_ADDRESS_MODE
RHI_ADDRESS_MODE(rhi::address_mode::repeat,               VK_SAMPLER_ADDRESS_MODE_REPEAT,               D3D11_TEXTURE_ADDRESS_WRAP,        GL_REPEAT)
RHI_ADDRESS_MODE(rhi::address_mode::mirrored_repeat,      VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,      D3D11_TEXTURE_ADDRESS_MIRROR,      GL_MIRRORED_REPEAT)
RHI_ADDRESS_MODE(rhi::address_mode::clamp_to_edge,        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,        D3D11_TEXTURE_ADDRESS_CLAMP,       GL_CLAMP_TO_EDGE)
RHI_ADDRESS_MODE(rhi::address_mode::mirror_clamp_to_edge, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE, GL_MIRROR_CLAMP_TO_EDGE)
RHI_ADDRESS_MODE(rhi::address_mode::clamp_to_border,      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,      D3D11_TEXTURE_ADDRESS_BORDER,      GL_CLAMP_TO_BORDER)
#undef RHI_ADDRESS_MODE
#endif   

// #define RHI_PRIMITIVE_TOPOLOGY(CASE, VK, DX, GL)
#ifdef RHI_PRIMITIVE_TOPOLOGY
RHI_PRIMITIVE_TOPOLOGY(rhi::primitive_topology::points,    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,    D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,    GL_POINTS)
RHI_PRIMITIVE_TOPOLOGY(rhi::primitive_topology::lines,     VK_PRIMITIVE_TOPOLOGY_LINE_LIST,     D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     GL_LINES)
RHI_PRIMITIVE_TOPOLOGY(rhi::primitive_topology::triangles, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, GL_TRIANGLES)
#undef RHI_PRIMITIVE_TOPOLOGY
#endif

// #define RHI_CULL_MODE(CASE, VK, DX, GL_ENABLED, GL_CULL_FACE)
#ifdef RHI_CULL_MODE
RHI_CULL_MODE(rhi::cull_mode::none,  VK_CULL_MODE_NONE,      D3D11_CULL_NONE,  false, GL_BACK)
RHI_CULL_MODE(rhi::cull_mode::back,  VK_CULL_MODE_BACK_BIT,  D3D11_CULL_BACK,  true,  GL_BACK)
RHI_CULL_MODE(rhi::cull_mode::front, VK_CULL_MODE_FRONT_BIT, D3D11_CULL_FRONT, true,  GL_FRONT)
#undef RHI_CULL_MODE
#endif

// #define RHI_BLEND_OP(CASE, VK, DX, GL)
#ifdef RHI_COMPARE_OP
RHI_COMPARE_OP(rhi::compare_op::never,            VK_COMPARE_OP_NEVER,            D3D11_COMPARISON_NEVER,         GL_NEVER)
RHI_COMPARE_OP(rhi::compare_op::less,             VK_COMPARE_OP_LESS,             D3D11_COMPARISON_LESS,          GL_LESS)
RHI_COMPARE_OP(rhi::compare_op::equal,            VK_COMPARE_OP_EQUAL,            D3D11_COMPARISON_EQUAL,         GL_EQUAL)
RHI_COMPARE_OP(rhi::compare_op::less_or_equal,    VK_COMPARE_OP_LESS_OR_EQUAL,    D3D11_COMPARISON_LESS_EQUAL,    GL_LEQUAL)
RHI_COMPARE_OP(rhi::compare_op::greater,          VK_COMPARE_OP_GREATER,          D3D11_COMPARISON_GREATER,       GL_GREATER)
RHI_COMPARE_OP(rhi::compare_op::not_equal,        VK_COMPARE_OP_NOT_EQUAL,        D3D11_COMPARISON_NOT_EQUAL,     GL_NOTEQUAL)
RHI_COMPARE_OP(rhi::compare_op::greater_or_equal, VK_COMPARE_OP_GREATER_OR_EQUAL, D3D11_COMPARISON_GREATER_EQUAL, GL_GEQUAL)
RHI_COMPARE_OP(rhi::compare_op::always,           VK_COMPARE_OP_ALWAYS,           D3D11_COMPARISON_ALWAYS,        GL_ALWAYS)
#undef RHI_COMPARE_OP
#endif

// #define RHI_BLEND_OP(CASE, VK, DX, GL)
#ifdef RHI_BLEND_OP
RHI_BLEND_OP(rhi::blend_op::add,              VK_BLEND_OP_ADD,              D3D11_BLEND_OP_ADD,           GL_FUNC_ADD)
RHI_BLEND_OP(rhi::blend_op::subtract,         VK_BLEND_OP_SUBTRACT,         D3D11_BLEND_OP_SUBTRACT,      GL_FUNC_SUBTRACT)
RHI_BLEND_OP(rhi::blend_op::reverse_subtract, VK_BLEND_OP_REVERSE_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT,  GL_FUNC_REVERSE_SUBTRACT)
RHI_BLEND_OP(rhi::blend_op::min,              VK_BLEND_OP_MIN,              D3D11_BLEND_OP_MIN,           GL_MIN)
RHI_BLEND_OP(rhi::blend_op::max,              VK_BLEND_OP_MAX,              D3D11_BLEND_OP_MAX,           GL_MAX)
#undef RHI_BLEND_OP
#endif

// #define RHI_BLEND_FACTOR(CASE, VK, DX, GL)
#ifdef RHI_BLEND_FACTOR
RHI_BLEND_FACTOR(rhi::blend_factor::zero,                     VK_BLEND_FACTOR_ZERO,                     D3D11_BLEND_ZERO,             GL_ZERO)
RHI_BLEND_FACTOR(rhi::blend_factor::one,                      VK_BLEND_FACTOR_ONE,                      D3D11_BLEND_ONE,              GL_ONE)
RHI_BLEND_FACTOR(rhi::blend_factor::constant_color,           VK_BLEND_FACTOR_CONSTANT_COLOR,           D3D11_BLEND_BLEND_FACTOR,     GL_CONSTANT_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::one_minus_constant_color, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, D3D11_BLEND_INV_BLEND_FACTOR, GL_ONE_MINUS_CONSTANT_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::source_color,             VK_BLEND_FACTOR_SRC_COLOR,                D3D11_BLEND_SRC_COLOR,        GL_SRC_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::one_minus_source_color,   VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,      D3D11_BLEND_INV_SRC_COLOR,    GL_ONE_MINUS_SRC_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::dest_color,               VK_BLEND_FACTOR_DST_COLOR,                D3D11_BLEND_DEST_COLOR,       GL_DST_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::one_minus_dest_color,     VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,      D3D11_BLEND_INV_DEST_COLOR,   GL_ONE_MINUS_DST_COLOR)
RHI_BLEND_FACTOR(rhi::blend_factor::source_alpha,             VK_BLEND_FACTOR_SRC_ALPHA,                D3D11_BLEND_SRC_ALPHA,        GL_SRC_ALPHA)
RHI_BLEND_FACTOR(rhi::blend_factor::one_minus_source_alpha,   VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,      D3D11_BLEND_INV_SRC_ALPHA,    GL_ONE_MINUS_SRC_ALPHA)
RHI_BLEND_FACTOR(rhi::blend_factor::dest_alpha,               VK_BLEND_FACTOR_DST_ALPHA,                D3D11_BLEND_DEST_ALPHA,       GL_DST_ALPHA)
RHI_BLEND_FACTOR(rhi::blend_factor::one_minus_dest_alpha,     VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,      D3D11_BLEND_INV_DEST_ALPHA,   GL_ONE_MINUS_DST_ALPHA)
#undef RHI_BLEND_FACTOR
#endif 

// #define RHI_ATTRIBUTE_FORMAT(CASE, VK, DX, GL_SIZE, GL_TYPE, GL_NORMALIZED)
#ifdef RHI_ATTRIBUTE_FORMAT
RHI_ATTRIBUTE_FORMAT(rhi::attribute_format::float1, VK_FORMAT_R32_SFLOAT,          DXGI_FORMAT_R32_FLOAT,          1, GL_FLOAT, GL_FALSE)
RHI_ATTRIBUTE_FORMAT(rhi::attribute_format::float2, VK_FORMAT_R32G32_SFLOAT,       DXGI_FORMAT_R32G32_FLOAT,       2, GL_FLOAT, GL_FALSE)
RHI_ATTRIBUTE_FORMAT(rhi::attribute_format::float3, VK_FORMAT_R32G32B32_SFLOAT,    DXGI_FORMAT_R32G32B32_FLOAT,    3, GL_FLOAT, GL_FALSE)
RHI_ATTRIBUTE_FORMAT(rhi::attribute_format::float4, VK_FORMAT_R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, GL_FLOAT, GL_FALSE)
#undef RHI_ATTRIBUTE_FORMAT
#endif

// #define RHI_IMAGE_FORMAT(CASE, SIZE, TYPE, VK, DX, GL_INTERNAL_FORMAT, GL_FORMAT, GL_TYPE)
#ifdef RHI_IMAGE_FORMAT
RHI_IMAGE_FORMAT(rhi::image_format::rgba_unorm8,            4*1, rhi::attachment_type::color,       VK_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,       GL_RGBA8,               GL_RGBA,            GL_UNSIGNED_BYTE)        
RHI_IMAGE_FORMAT(rhi::image_format::rgba_srgb8,             4*1, rhi::attachment_type::color,       VK_FORMAT_R8G8B8A8_SRGB,       DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,  GL_SRGB8_ALPHA8,        GL_RGBA,            GL_UNSIGNED_BYTE) 
RHI_IMAGE_FORMAT(rhi::image_format::rgba_norm8,             4*1, rhi::attachment_type::color,       VK_FORMAT_R8G8B8A8_SNORM,      DXGI_FORMAT_R8G8B8A8_SNORM,       GL_RGBA8_SNORM,         GL_RGBA,            GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_uint8,             4*1, rhi::attachment_type::color,       VK_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_R8G8B8A8_UINT,        GL_RGBA8UI,             GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_int8,              4*1, rhi::attachment_type::color,       VK_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_R8G8B8A8_SINT,        GL_RGBA8I,              GL_RGBA_INTEGER,    GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_unorm16,           4*2, rhi::attachment_type::color,       VK_FORMAT_R16G16B16A16_UNORM,  DXGI_FORMAT_R16G16B16A16_UNORM,   GL_RGBA16,              GL_RGBA,            GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_norm16,            4*2, rhi::attachment_type::color,       VK_FORMAT_R16G16B16A16_SNORM,  DXGI_FORMAT_R16G16B16A16_SNORM,   GL_RGBA16_SNORM,        GL_RGBA,            GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_uint16,            4*2, rhi::attachment_type::color,       VK_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_R16G16B16A16_UINT,    GL_RGBA16UI,            GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_int16,             4*2, rhi::attachment_type::color,       VK_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_R16G16B16A16_SINT,    GL_RGBA16I,             GL_RGBA_INTEGER,    GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_float16,           4*2, rhi::attachment_type::color,       VK_FORMAT_R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,   GL_RGBA16F,             GL_RGBA,            GL_HALF_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_uint32,            4*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_R32G32B32A32_UINT,    GL_RGBA32UI,            GL_RGBA_INTEGER,    GL_UNSIGNED_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_int32,             4*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_R32G32B32A32_SINT,    GL_RGBA32I,             GL_RGBA_INTEGER,    GL_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rgba_float32,           4*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,   GL_RGBA32F,             GL_RGBA,            GL_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::rgb_uint32,             3*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32_UINT,      DXGI_FORMAT_R32G32B32_UINT,       GL_RGB32UI,             GL_RGB_INTEGER,     GL_UNSIGNED_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rgb_int32,              3*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32_SINT,      DXGI_FORMAT_R32G32B32_SINT,       GL_RGB32I,              GL_RGB_INTEGER,     GL_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rgb_float32,            3*4, rhi::attachment_type::color,       VK_FORMAT_R32G32B32_SFLOAT,    DXGI_FORMAT_R32G32B32_FLOAT,      GL_RGB32F,              GL_RGB,             GL_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_unorm8,              2*1, rhi::attachment_type::color,       VK_FORMAT_R8G8_UNORM,          DXGI_FORMAT_R8G8_UNORM,           GL_RG8,                 GL_RG,              GL_UNSIGNED_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rg_norm8,               2*1, rhi::attachment_type::color,       VK_FORMAT_R8G8_SNORM,          DXGI_FORMAT_R8G8_SNORM,           GL_RG8_SNORM,           GL_RG,              GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rg_uint8,               2*1, rhi::attachment_type::color,       VK_FORMAT_R8G8_UINT,           DXGI_FORMAT_R8G8_UINT,            GL_RG8UI,               GL_RG_INTEGER,      GL_UNSIGNED_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rg_int8,                2*1, rhi::attachment_type::color,       VK_FORMAT_R8G8_SINT,           DXGI_FORMAT_R8G8_SINT,            GL_RG8I,                GL_RG_INTEGER,      GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::rg_unorm16,             2*2, rhi::attachment_type::color,       VK_FORMAT_R16G16_UNORM,        DXGI_FORMAT_R16G16_UNORM,         GL_RG16,                GL_RG,              GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_norm16,              2*2, rhi::attachment_type::color,       VK_FORMAT_R16G16_SNORM,        DXGI_FORMAT_R16G16_SNORM,         GL_RG16_SNORM,          GL_RG,              GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_uint16,              2*2, rhi::attachment_type::color,       VK_FORMAT_R16G16_UINT,         DXGI_FORMAT_R16G16_UINT,          GL_RG16UI,              GL_RG_INTEGER,      GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_int16,               2*2, rhi::attachment_type::color,       VK_FORMAT_R16G16_SINT,         DXGI_FORMAT_R16G16_SINT,          GL_RG16I,               GL_RG_INTEGER,      GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_float16,             2*2, rhi::attachment_type::color,       VK_FORMAT_R16G16_SFLOAT,       DXGI_FORMAT_R16G16_FLOAT,         GL_RG16F,               GL_RG,              GL_HALF_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_uint32,              2*4, rhi::attachment_type::color,       VK_FORMAT_R32G32_UINT,         DXGI_FORMAT_R32G32_UINT,          GL_RG32UI,              GL_RG_INTEGER,      GL_UNSIGNED_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_int32,               2*4, rhi::attachment_type::color,       VK_FORMAT_R32G32_SINT,         DXGI_FORMAT_R32G32_SINT,          GL_RG32I,               GL_RG_INTEGER,      GL_INT)
RHI_IMAGE_FORMAT(rhi::image_format::rg_float32,             2*4, rhi::attachment_type::color,       VK_FORMAT_R32G32_SFLOAT,       DXGI_FORMAT_R32G32_FLOAT,         GL_RG32F,               GL_RG,              GL_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::r_unorm8,               1*1, rhi::attachment_type::color,       VK_FORMAT_R8_UNORM,            DXGI_FORMAT_R8_UNORM,             GL_R8,                  GL_RED,             GL_UNSIGNED_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::r_norm8,                1*1, rhi::attachment_type::color,       VK_FORMAT_R8_SNORM,            DXGI_FORMAT_R8_SNORM,             GL_R8_SNORM,            GL_RED,             GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::r_uint8,                1*1, rhi::attachment_type::color,       VK_FORMAT_R8_UINT,             DXGI_FORMAT_R8_UINT,              GL_R8UI,                GL_RED_INTEGER,     GL_UNSIGNED_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::r_int8,                 1*1, rhi::attachment_type::color,       VK_FORMAT_R8G8_SINT,           DXGI_FORMAT_R8_SINT,              GL_R8I,                 GL_RED_INTEGER,     GL_BYTE)
RHI_IMAGE_FORMAT(rhi::image_format::r_unorm16,              1*2, rhi::attachment_type::color,       VK_FORMAT_R16_UNORM,           DXGI_FORMAT_R16_UNORM,            GL_R16,                 GL_RED,             GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::r_norm16,               1*2, rhi::attachment_type::color,       VK_FORMAT_R16_SNORM,           DXGI_FORMAT_R16_SNORM,            GL_R16_SNORM,           GL_RED,             GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::r_uint16,               1*2, rhi::attachment_type::color,       VK_FORMAT_R16_UINT,            DXGI_FORMAT_R16_UINT,             GL_R16UI,               GL_RED_INTEGER,     GL_UNSIGNED_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::r_int16,                1*2, rhi::attachment_type::color,       VK_FORMAT_R16_SINT,            DXGI_FORMAT_R16_SINT,             GL_R16I,                GL_RED_INTEGER,     GL_SHORT)
RHI_IMAGE_FORMAT(rhi::image_format::r_float16,              1*2, rhi::attachment_type::color,       VK_FORMAT_R16_SFLOAT,          DXGI_FORMAT_R16_FLOAT,            GL_R16F,                GL_RED,             GL_HALF_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::r_uint32,               1*4, rhi::attachment_type::color,       VK_FORMAT_R32_UINT,            DXGI_FORMAT_R32_UINT,             GL_R32UI,               GL_RED_INTEGER,     GL_UNSIGNED_INT)
RHI_IMAGE_FORMAT(rhi::image_format::r_int32,                1*4, rhi::attachment_type::color,       VK_FORMAT_R32_SINT,            DXGI_FORMAT_R32_SINT,             GL_R32I,                GL_RED_INTEGER,     GL_INT)
RHI_IMAGE_FORMAT(rhi::image_format::r_float32,              1*4, rhi::attachment_type::color,       VK_FORMAT_R32_SFLOAT,          DXGI_FORMAT_R32_FLOAT,            GL_R32F,                GL_RED,             GL_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::depth_unorm16,          2, rhi::attachment_type::depth_stencil, VK_FORMAT_D16_UNORM,           DXGI_FORMAT_D16_UNORM,            GL_DEPTH_COMPONENT16,   GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT) 
RHI_IMAGE_FORMAT(rhi::image_format::depth_unorm24_stencil8, 4, rhi::attachment_type::depth_stencil, VK_FORMAT_D24_UNORM_S8_UINT,   DXGI_FORMAT_D24_UNORM_S8_UINT,    GL_DEPTH24_STENCIL8,    GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8)
RHI_IMAGE_FORMAT(rhi::image_format::depth_float32,          4, rhi::attachment_type::depth_stencil, VK_FORMAT_D32_SFLOAT,          DXGI_FORMAT_D32_FLOAT,            GL_DEPTH_COMPONENT32F,  GL_DEPTH_COMPONENT, GL_FLOAT)
RHI_IMAGE_FORMAT(rhi::image_format::depth_float32_stencil8, 8, rhi::attachment_type::depth_stencil, VK_FORMAT_D32_SFLOAT_S8_UINT,  DXGI_FORMAT_D32_FLOAT_S8X24_UINT, GL_DEPTH32F_STENCIL8,   GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV) 
#undef RHI_IMAGE_FORMAT
#endif