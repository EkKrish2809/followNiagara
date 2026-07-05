struct Vertex{
    float vx, vy, vz;
    uint8_t nx, ny, nz, nw;
    float16_t tu, tv;
};

struct Meshlet{
    vec4 cone;
    uint dataOffset;
    uint8_t vertexCount;
    uint8_t triangleCount;
    // uint vertices[64];
    // // uint8_t indices[124*3];   // up to 126 triangles
    // uint indicesPacked[124*3/4];   // up to 126 triangles
};

struct MeshDraw{
    mat4 projection;
    vec3 position;
    float scale;
    vec4 orientation;
    // vec2 offset;
    // vec2 scale;
};


vec3 rotateQuat(vec3 v, vec4 q){
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}