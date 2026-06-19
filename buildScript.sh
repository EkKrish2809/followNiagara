rm ./a.out

glslc src/triangle.vert -o src/triangle.vert.spv

glslc src/triangle.frag -o src/triangle.frag.spv

FILES=(
    src/niagara.cpp

    # include/volk/volk.c
    include/fast_obj/fast_obj.c
    meshoptimizer/src/*.cpp

)

g++ "${FILES[@]}" -g3 -lglfw -lvulkan -lX11

./a.out asset/kitten.obj