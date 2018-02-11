[![License is Unlicense](http://img.shields.io/badge/license-Unlicense-blue.svg?style=flat)](http://unlicense.org/)
[![Build status](https://ci.appveyor.com/api/projects/status/74wvkherug11jafc?svg=true)](https://ci.appveyor.com/project/sgorsten/workbench)

Just some random programming experiments, based on some work in a few of my other repositories as well as some new stuff. Everything is work-in-progress, but some design notes and rationales are available [here](doc/design.md)

### Dependencies

* [Visual Studio 2017](https://www.visualstudio.com/downloads/)
* [LunarG Vulkan SDK](http://www.lunarg.com/vulkan-sdk/)

### Licensing

A reasonable effort has been made to limit the contents of the `src/` directory to my original work, which is available under the terms of the [Unlicense](http://unlicense.org/)

The contents of the `dep/` directory are taken from other open source projects and are available under the terms of their respective licenses:

* `dep/glad/`: [GLAD](http://github.com/Dav1dde/glad) is licensed under the MIT license
* `dep/glfw-3.2.1/`: [GLFW](http://www.glfw.org/) is licensed under the zlib/libpng license
* `dep/glslang/`: [glslang](http://github.com/KhronosGroup/glslang) is licensed under the BSD license
* `dep/SPIRV-Cross/`: [SPIRV-Cross](http://github.com/KhronosGroup/SPIRV-Cross) is licensed under the Apache License 2.0
* `dep/include/doctest.h`: [doctest](http://github.com/onqtam/doctest) is licensed under the MIT license
* `dep/include/stb_image.h`: [stb](https://github.com/nothings/stb) is in the public domain
* `dep/include/linalg.h`: [linalg](http://github.com/sgorsten/linalg) is unlicensed

The contents of the `assets/` directory are taken from royalty-free sources and may have been reduced in resolution to minimize repository bloat. Please visit the author's website to obtain the original, full resolution files:

* `assets/monument-valley.hdr`: The [sIBL Archive](http://www.hdrlabs.com/sibl/archive.html) is licensed under the Creative Commons Attribution-Noncommercial-Share Alike 3.0 License
* `assets/fontawesome-webfont.ttf`:  The [font-awesome-4.5.0 package](http://fontawesome.io/) by Dave Gandy is licensed under the SIL Open Font License 1.1
* `assets/proggy-clean.pcf`: The [Proggy Fonts](https://proggyfonts.net/) by Tristan Grimmer are licensed under the following license:

    ```
    Copyright (c) 2004, 2005 Tristan Grimmer
    
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
    ```