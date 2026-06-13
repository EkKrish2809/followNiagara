rm ./a.out

glslc src/triangle.vert -o src/triangle.vert.spv

glslc src/triangle.frag -o src/triangle.frag.spv

FILES=(
    src/niagara.cpp
)

g++ "${FILES[@]}" -g3 -lglfw -lvulkan -lX11

./a.out