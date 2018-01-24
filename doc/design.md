# Modules

## Core (core.h)

Provides vocabulary types and useful utilities that will effectively be universally included across the entire engine. Generally, it should only include the standard library, but we have made a specific exception for `doctest.h`, which will be enabled in debug mode and disabled in release mode. This is to encourage the writing of unit tests in all modules, close to the implementations of the functions they are testing. However, we unconditionally disable the "short" macro names, and use only the `DOCTEST_*` prefixed macros.

## Loader (load.h)

Provides a single point of access to the filesystem, and abstracts over the concept of external resources. At the moment, it simply supports reading text and binary files, as well as some image formats. Eventually, we want to support features such as packages, mods, and hotloading.

## Render Hardware Interface (rhi.h)

Exposes a single simple, coherent interface to rendering hardware, with backing implementations for Vulkan, Direct3D 11, and OpenGL 4.5. The design deliberately prioritizes ease of implementation over ease of use, as every type, function, and enumerant added to the RHI generally requires code to be modified in a half dozen places. One notable concession to usability is that all device object interfaces exposed by the RHI are internally reference-counted, allowing logic both above and below the RHI layer to influence the lifetime of device objects.

Stylistically, the RHI is a command-buffer oriented API. Command buffers are recorded ahead of time and then submitted to the device for execution. Care must be taken to avoid invalidating shader resources referenced by outstanding command buffers, but the RHI provides some simple fencing mechanisms to accommodate this.

Shader logic is expressed in SPIR-V following Vulkan rules, where all uniforms must be part of a block, and shader resources are organized into sets. The RHI borrows the concept of descriptor sets from Vulkan essentially verbatim, and emulates them on all other backends. Like Vulkan and other modern APIs, shaders and fixed function state must be compiled into pipelines prior to use. However, unlike Vulkan, the RHI does not require you to define your render passes ahead of time or specify render pass information when creating pipelines or framebuffers.

## Graphics (gfx.h)

A set of (slightly) higher level interfaces built on top of RHI objects. This module is intended to be tightly coupled to the RHI, but expose an interface that prioritizes usability for the developer. As code inside this module has been fully insulated from the details of the underlying rendering API, it only needs to be written once, and as such there is no upper limit to the level of sophistication that might be added.

However, the aim for now will be to focus on concepts such as meshes, instancing, particles, streaming data, shader/pipeline layout introspection, and the provisioning and tracking of temporary resources used during rendering. Where the RHI requires command buffers to reference descriptor sets allocated out of pools according to specific layouts, which themselves reference separately allocated images and buffers, the graphics module may provide a higher level command buffer where you bind a pipeline, assign values to per-object uniforms and samplers by name, and then issue a command to draw one or more meshes.

Details about lighting models or specific postprocessing algorithms should be left to a higher module still

## PBR (pbr.h)

A set of standard resources (shaders, layouts, etc.) for implementing a physically based renderer. This includes a set of shader includes for authoring PBR shaders, as well as a set of tools for precomputing resources used during the lighting face, such as irradiance and reflectance cubemaps.

## Sprite (sprite.h)

A set of functions and data structures for rendering of UIs based on glyphs and 2D shapes, which can be used for rendering both immediate-mode and retained-mode GUIs.

## Other

* `geometry.h` - Defines a rigid transform class and a facility for mapping between coordinate systems
* `mesh.h` - Defines a standard vertex format and procedural generation of various types of meshes
* `camera.h` - Defines a standard WASD+mouselook style camera 
