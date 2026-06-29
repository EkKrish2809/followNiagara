rm ./a.out

glslc src/shaders/mesh.vert -o src/shaders/mesh.vert.spv

glslc src/shaders/meshlet.task -c --target-spv=spv1.3 -o src/shaders/meshlet.task.spv
glslc src/shaders/meshlet.mesh -o src/shaders/meshlet.mesh.spv

glslc src/shaders/mesh.frag -o src/shaders/mesh.frag.spv

FILES=(
    src/niagara.cpp
    src/shaders.cpp

    # include/volk/volk.c
    include/fast_obj/fast_obj.c
    meshoptimizer/src/*.cpp

)

g++ "${FILES[@]}" -g3 -lglfw -lvulkan -lX11

./a.out asset/kitten.obj