rm ./a.out

FILES=(
    src/niagara.cpp
)

g++ "${FILES[@]}" -g3 -lglfw -lvulkan -lX11

./a.out