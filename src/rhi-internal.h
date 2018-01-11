#pragma once
#include "rhi.h"

template<class HANDLE, class TYPE> class object_set
{
    std::unordered_map<int, TYPE> objects;
    int next_id = 1;
public:
    std::tuple<HANDLE, TYPE &> create() 
    { 
        const int id = next_id++;
        return {HANDLE{id}, objects[id]};
    }

    TYPE & operator[] (HANDLE h) 
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
    }
};