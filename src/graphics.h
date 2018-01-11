#pragma once
#include "rhi.h"

#include <type_traits>
struct binary_view 
{ 
    size_t size; const void * data; 
    binary_view(size_t size, const void * data) : size{size}, data{data} {}
    template<class T> binary_view(const T & obj) : binary_view{sizeof(T), &obj} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    template<class T> binary_view(const std::vector<T> & vec) : binary_view{vec.size()*sizeof(T), vec.data()} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    // TODO: std::array, C array, etc.
};

inline rhi::buffer_range make_static_buffer(rhi::device & dev, rhi::buffer_usage usage, binary_view contents)
{
    return {std::get<rhi::buffer>(dev.create_buffer({contents.size, usage, false}, contents.data)), 0, contents.size};
}

class dynamic_buffer
{
    rhi::buffer buffer;
    char * mapped;
    size_t used;
public:
    dynamic_buffer(rhi::device & dev, rhi::buffer_usage usage, size_t size)
    {
        std::tie(buffer, mapped) = dev.create_buffer({size, usage, true}, nullptr);
    }

    void reset()
    {
        used = 0;
    }

    rhi::buffer_range write(binary_view contents)
    {
        // TODO: Check for buffer exhaustion
        const rhi::buffer_range range {buffer, (used+255)/256*256, contents.size};
        memcpy(mapped + range.offset, contents.data, static_cast<size_t>(range.size));
        used = range.offset + range.size;
        return range;
    }
};
