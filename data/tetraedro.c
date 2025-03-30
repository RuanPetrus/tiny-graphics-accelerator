Vec3 tetraedro_vert[4] = {
    { -0.5, 0 ,-0.5 },
    { 0.5,  0 ,-0.5 },
    { 0, 0, 0.5 },
    { 0, 0.5, 0 }
};
Face tetraedro_faces[4] = {
    { 0,1,2 },
    { 0,1,3 },
    { 0,2,3 },
    { 1,2,3 }
};
const Mesh tetraedro_mesh = { tetraedro_vert, 4, tetraedro_faces, 4 };
