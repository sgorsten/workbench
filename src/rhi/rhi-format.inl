// #define X(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT)
X(rhi::image_format::rgba_unorm8,  4*1, rhi::attachment_type::color, VK_FORMAT_R8G8B8A8_UNORM,      DXGI_FORMAT_R8G8B8A8_UNORM,      GL_RGBA8,        GL_RGBA,         GL_UNSIGNED_BYTE)        
X(rhi::image_format::rgba_srgb8,   4*1, rhi::attachment_type::color, VK_FORMAT_R8G8B8A8_SRGB,       DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, GL_SRGB8_ALPHA8, GL_RGBA,         GL_UNSIGNED_BYTE) 
X(rhi::image_format::rgba_norm8,   4*1, rhi::attachment_type::color, VK_FORMAT_R8G8B8A8_SNORM,      DXGI_FORMAT_R8G8B8A8_SNORM,      GL_RGBA8_SNORM,  GL_RGBA,         GL_BYTE)
X(rhi::image_format::rgba_uint8,   4*1, rhi::attachment_type::color, VK_FORMAT_R8G8B8A8_UINT,       DXGI_FORMAT_R8G8B8A8_UINT,       GL_RGBA8UI,      GL_RGBA_INTEGER, GL_UNSIGNED_BYTE)
X(rhi::image_format::rgba_int8,    4*1, rhi::attachment_type::color, VK_FORMAT_R8G8B8A8_SINT,       DXGI_FORMAT_R8G8B8A8_SINT,       GL_RGBA8I,       GL_RGBA_INTEGER, GL_BYTE)
X(rhi::image_format::rgba_unorm16, 4*2, rhi::attachment_type::color, VK_FORMAT_R16G16B16A16_UNORM,  DXGI_FORMAT_R16G16B16A16_UNORM,  GL_RGBA16,       GL_RGBA,         GL_UNSIGNED_SHORT)
X(rhi::image_format::rgba_norm16,  4*2, rhi::attachment_type::color, VK_FORMAT_R16G16B16A16_SNORM,  DXGI_FORMAT_R16G16B16A16_SNORM,  GL_RGBA16_SNORM, GL_RGBA,         GL_SHORT)
X(rhi::image_format::rgba_uint16,  4*2, rhi::attachment_type::color, VK_FORMAT_R16G16B16A16_UINT,   DXGI_FORMAT_R16G16B16A16_UINT,   GL_RGBA16UI,     GL_RGBA_INTEGER, GL_UNSIGNED_SHORT)
X(rhi::image_format::rgba_int16,   4*2, rhi::attachment_type::color, VK_FORMAT_R16G16B16A16_SINT,   DXGI_FORMAT_R16G16B16A16_SINT,   GL_RGBA16I,      GL_RGBA_INTEGER, GL_SHORT)
X(rhi::image_format::rgba_float16, 4*2, rhi::attachment_type::color, VK_FORMAT_R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,  GL_RGBA16F,      GL_RGBA,         GL_HALF_FLOAT)
X(rhi::image_format::rgba_uint32,  4*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_R32G32B32A32_UINT,   GL_RGBA32UI,     GL_RGBA_INTEGER, GL_UNSIGNED_INT)
X(rhi::image_format::rgba_int32,   4*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_R32G32B32A32_SINT,   GL_RGBA32I,      GL_RGBA_INTEGER, GL_INT)
X(rhi::image_format::rgba_float32, 4*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,  GL_RGBA32F,      GL_RGBA,         GL_FLOAT)
X(rhi::image_format::rgb_uint32,   3*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32_UINT,      DXGI_FORMAT_R32G32B32_UINT,      GL_RGB32UI,      GL_RGB_INTEGER,  GL_UNSIGNED_INT)
X(rhi::image_format::rgb_int32,    3*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32_SINT,      DXGI_FORMAT_R32G32B32_SINT,      GL_RGB32I,       GL_RGB_INTEGER,  GL_INT)
X(rhi::image_format::rgb_float32,  3*4, rhi::attachment_type::color, VK_FORMAT_R32G32B32_SFLOAT,    DXGI_FORMAT_R32G32B32_FLOAT,     GL_RGB32F,       GL_RGB,          GL_FLOAT)
X(rhi::image_format::rg_unorm8,    2*1, rhi::attachment_type::color, VK_FORMAT_R8G8_UNORM,          DXGI_FORMAT_R8G8_UNORM,          GL_RG8,          GL_RG,           GL_UNSIGNED_BYTE)
X(rhi::image_format::rg_norm8,     2*1, rhi::attachment_type::color, VK_FORMAT_R8G8_SNORM,          DXGI_FORMAT_R8G8_SNORM,          GL_RG8_SNORM,    GL_RG,           GL_BYTE)
X(rhi::image_format::rg_uint8,     2*1, rhi::attachment_type::color, VK_FORMAT_R8G8_UINT,           DXGI_FORMAT_R8G8_UINT,           GL_RG8UI,        GL_RG_INTEGER,   GL_UNSIGNED_BYTE)
X(rhi::image_format::rg_int8,      2*1, rhi::attachment_type::color, VK_FORMAT_R8G8_SINT,           DXGI_FORMAT_R8G8_SINT,           GL_RG8I,         GL_RG_INTEGER,   GL_BYTE)
X(rhi::image_format::rg_unorm16,   2*2, rhi::attachment_type::color, VK_FORMAT_R16G16_UNORM,        DXGI_FORMAT_R16G16_UNORM,        GL_RG16,         GL_RG,           GL_UNSIGNED_SHORT)
X(rhi::image_format::rg_norm16,    2*2, rhi::attachment_type::color, VK_FORMAT_R16G16_SNORM,        DXGI_FORMAT_R16G16_SNORM,        GL_RG16_SNORM,   GL_RG,           GL_SHORT)
X(rhi::image_format::rg_uint16,    2*2, rhi::attachment_type::color, VK_FORMAT_R16G16_UINT,         DXGI_FORMAT_R16G16_UINT,         GL_RG16UI,       GL_RG_INTEGER,   GL_UNSIGNED_SHORT)
X(rhi::image_format::rg_int16,     2*2, rhi::attachment_type::color, VK_FORMAT_R16G16_SINT,         DXGI_FORMAT_R16G16_SINT,         GL_RG16I,        GL_RG_INTEGER,   GL_SHORT)
X(rhi::image_format::rg_float16,   2*2, rhi::attachment_type::color, VK_FORMAT_R16G16_SFLOAT,       DXGI_FORMAT_R16G16_FLOAT,        GL_RG16F,        GL_RG,           GL_HALF_FLOAT)
X(rhi::image_format::rg_uint32,    2*4, rhi::attachment_type::color, VK_FORMAT_R32G32_UINT,         DXGI_FORMAT_R32G32_UINT,         GL_RG32UI,       GL_RG_INTEGER,   GL_UNSIGNED_INT)
X(rhi::image_format::rg_int32,     2*4, rhi::attachment_type::color, VK_FORMAT_R32G32_SINT,         DXGI_FORMAT_R32G32_SINT,         GL_RG32I,        GL_RG_INTEGER,   GL_INT)
X(rhi::image_format::rg_float32,   2*4, rhi::attachment_type::color, VK_FORMAT_R32G32_SFLOAT,       DXGI_FORMAT_R32G32_FLOAT,        GL_RG32F,        GL_RG,           GL_FLOAT)
X(rhi::image_format::r_unorm8,     1*1, rhi::attachment_type::color, VK_FORMAT_R8_UNORM,            DXGI_FORMAT_R8_UNORM,            GL_R8,           GL_RED,          GL_UNSIGNED_BYTE)
X(rhi::image_format::r_norm8,      1*1, rhi::attachment_type::color, VK_FORMAT_R8_SNORM,            DXGI_FORMAT_R8_SNORM,            GL_R8_SNORM,     GL_RED,          GL_BYTE)
X(rhi::image_format::r_uint8,      1*1, rhi::attachment_type::color, VK_FORMAT_R8_UINT,             DXGI_FORMAT_R8_UINT,             GL_R8UI,         GL_RED_INTEGER,  GL_UNSIGNED_BYTE)
X(rhi::image_format::r_int8,       1*1, rhi::attachment_type::color, VK_FORMAT_R8G8_SINT,           DXGI_FORMAT_R8_SINT,             GL_R8I,          GL_RED_INTEGER,  GL_BYTE)
X(rhi::image_format::r_unorm16,    1*2, rhi::attachment_type::color, VK_FORMAT_R16_UNORM,           DXGI_FORMAT_R16_UNORM,           GL_R16,          GL_RED,          GL_UNSIGNED_SHORT)
X(rhi::image_format::r_norm16,     1*2, rhi::attachment_type::color, VK_FORMAT_R16_SNORM,           DXGI_FORMAT_R16_SNORM,           GL_R16_SNORM,    GL_RED,          GL_SHORT)
X(rhi::image_format::r_uint16,     1*2, rhi::attachment_type::color, VK_FORMAT_R16_UINT,            DXGI_FORMAT_R16_UINT,            GL_R16UI,        GL_RED_INTEGER,  GL_UNSIGNED_SHORT)
X(rhi::image_format::r_int16,      1*2, rhi::attachment_type::color, VK_FORMAT_R16_SINT,            DXGI_FORMAT_R16_SINT,            GL_R16I,         GL_RED_INTEGER,  GL_SHORT)
X(rhi::image_format::r_float16,    1*2, rhi::attachment_type::color, VK_FORMAT_R16_SFLOAT,          DXGI_FORMAT_R16_FLOAT,           GL_R16F,         GL_RED,          GL_HALF_FLOAT)
X(rhi::image_format::r_uint32,     1*4, rhi::attachment_type::color, VK_FORMAT_R32_UINT,            DXGI_FORMAT_R32_UINT,            GL_R32UI,        GL_RED_INTEGER,  GL_UNSIGNED_INT)
X(rhi::image_format::r_int32,      1*4, rhi::attachment_type::color, VK_FORMAT_R32_SINT,            DXGI_FORMAT_R32_SINT,            GL_R32I,         GL_RED_INTEGER,  GL_INT)
X(rhi::image_format::r_float32,    1*4, rhi::attachment_type::color, VK_FORMAT_R32_SFLOAT,          DXGI_FORMAT_R32_FLOAT,           GL_R32F,         GL_RED,          GL_FLOAT)
X(rhi::image_format::depth_unorm16,          2, rhi::attachment_type::depth_stencil, VK_FORMAT_D16_UNORM,          DXGI_FORMAT_D16_UNORM,            GL_DEPTH_COMPONENT16,   GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT) 
X(rhi::image_format::depth_unorm24_stencil8, 4, rhi::attachment_type::depth_stencil, VK_FORMAT_D24_UNORM_S8_UINT,  DXGI_FORMAT_D24_UNORM_S8_UINT,    GL_DEPTH24_STENCIL8,    GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8)
X(rhi::image_format::depth_float32,          4, rhi::attachment_type::depth_stencil, VK_FORMAT_D32_SFLOAT,         DXGI_FORMAT_D32_FLOAT,            GL_DEPTH_COMPONENT32F,  GL_DEPTH_COMPONENT, GL_FLOAT)
X(rhi::image_format::depth_float32_stencil8, 8, rhi::attachment_type::depth_stencil, VK_FORMAT_D32_SFLOAT_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, GL_DEPTH32F_STENCIL8,   GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV) 
