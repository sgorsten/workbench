#pragma once
#include "rhi.h"

template<class HANDLE, class TYPE> struct object_set
{
    std::unordered_map<int, TYPE> objects;
    int next_id = 1;

    HANDLE alloc() { return HANDLE{next_id++}; }
    TYPE & operator[] (HANDLE h) { return objects[h.id]; }
};