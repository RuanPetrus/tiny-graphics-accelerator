Vec3 triangle_vert[3] = {
    { 0.0,-0.5,0.0 },
    { -0.5,0.5,0.0 },
    { 0.5,0.5,0.0 }
};
Face triangle_faces[1] = {
    { 0,1,2 }
};
const Mesh triangle_mesh = { triangle_vert, 3, triangle_faces, 1 };
