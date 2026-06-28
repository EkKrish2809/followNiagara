* Compile C++ to SPIR-V. eliminate shaders ??
https://www.youtube.com/watch?v=g3tTUzPmHwM
====================================================================================================================
Application Layout

# 1. 


# 2. Mesh shaders
>> convert mesh into tiny "meshlets" , which are then passed to Task Shader

We Need :
    1) Array of transformed vertex data
    2) Array of triangles 8-bit indices
        Vertex buffer (we already have it)
        meshlet buffer of 8-bit indices
        

Task Shader -> Mesh Shader -> | Rasterizer | 