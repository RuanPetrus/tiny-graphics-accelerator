Vec3 triangle2_vert[6] = {
    { 0.0,0.8,0.0 },
    { -0.8,-0.8,0.0 },
    { 0.8,-0.8,0.0 },
    { 0.0,0.8,0.0 },
    { 0.0,-0.8,-0.8 },
    { 0.0,-0.8,0.8 }
};
Face triangle2_faces[2] = {
    { 0,1,2 },
    { 3,4,5 }
};
const Mesh triangle2_mesh = { triangle2_vert, 6, triangle2_faces, 2 };
