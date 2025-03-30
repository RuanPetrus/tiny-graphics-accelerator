#include <stdio.h>
#include <math.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "VRasterizer.h"

const int START_SIGNAL_TIME = 1;

#define WINDOW_WIDTH 320
#define WINDOW_STRIDE 320
#define WINDOW_HEIGHT 240

void tick(VRasterizer *tb) {
	tb->eval();
	tb->i_clk = 1;
	tb->eval();
	tb->i_clk = 0;
	tb->eval();
}

VRasterizer *tb = new VRasterizer();

typedef void (*RasterizationFunction)(
				  int32_t x1, int32_t x2, int32_t x3,
				  int32_t y1, int32_t y2, int32_t y3,
				  int32_t iz1, int32_t iz2, int32_t iz3,
				  uint32_t *canvas, int32_t *zbuffer, uint32_t color);


void circuit_draw(
				  int32_t x1, int32_t x2, int32_t x3,
				  int32_t y1, int32_t y2, int32_t y3,
				  int32_t iz1, int32_t iz2, int32_t iz3,
				  uint32_t *canvas, int32_t *zbuffer, uint32_t color) 
{
	uint64_t v1 = ((uint64_t) ((uint32_t) y1) << 32) | ((uint32_t) x1);
	uint64_t v2 = ((uint64_t) ((uint32_t) y2) << 32) | ((uint32_t) x2);
	uint64_t v3 = ((uint64_t) ((uint32_t) y3) << 32) | ((uint32_t) x3);
	tb->i_v1 = v1;
	tb->i_v2 = v2;
	tb->i_v3 = v3;
	tb->i_iz1 = iz1;
	tb->i_iz2 = iz2;
	tb->i_iz3 = iz3;

	bool start = false;
	bool finished = false;
	int32_t count = 0;

	assert(tb->o_done);
	for (int k = 0; !finished; k++) {
		if (START_SIGNAL_TIME == k) tb->i_start = 1, start = true;
		else                        tb->i_start = 0;

		tb->i_zbuffer_data = zbuffer[tb->o_zbuffer_addr];
		tick(tb);

		if (tb->o_write) {
			int32_t y = (tb->o_point >> 16) & 0xFFFF;
			int32_t x = (tb->o_point >> 0)  & 0xFFFF;

			assert(0 <= y && y < WINDOW_HEIGHT);
			assert(0 <= x && x < WINDOW_WIDTH);

			zbuffer[y*WINDOW_WIDTH + x] = tb->o_zbuffer_data;
			canvas[y*WINDOW_WIDTH + x] = color;
		}
		if (start && tb->o_done) finished = true;
		count += start;
	}
}

typedef uint32_t  u32;
typedef uint16_t  u16;
typedef uint8_t   u8;
typedef int64_t   i64;
typedef int32_t   i32;
typedef int16_t   i16;
typedef int8_t    i8;

typedef float f32;
typedef long double f64;

typedef u32* canvas_ptr;
typedef u32 PixelType;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define SWAP(T, a, b) do { T temp = a; a = b; b = temp; } while(0)
#define max3(a, b, c) max(a, max(b, c))
#define min3(a, b, c) min(a, min(b, c))
#define PI 3.14159265358979323846f


#define GREEN 0x00FF00FF
#define RED 0xFF0000FF
#define BLUE 0x0000FFFF
#define YELLOW 0xFFFF00FF
#define PURPLE 0xFF00FFFF
#define CYAN 0x00FFFFFF

#define WHITE 0xFFFFFFFF

#define PLAYER_SPEED 0.7f
#define ROTATION_SPEED 1.5f
#define DELTA 0.00001f
#define Z_FRACTION_BITS 16
#define NEAR_PLANE 0.20f
#define FAR_PLANE 16.0f
const int32_t INV_NEAR_PLANE = ((1 << Z_FRACTION_BITS) * (1.0f/NEAR_PLANE));
	const int32_t INV_FAR_PLANE = ((1 << Z_FRACTION_BITS) * (1.0f/FAR_PLANE));

typedef struct Vec2 Vec2;
struct Vec2 {
	f32 x, y;
};

typedef struct Vec3 Vec3;
struct Vec3 {
	f32 x, y, z;
};

typedef struct Face Face;
struct Face {
	u32 v1, v2, v3;
};

typedef struct Mesh Mesh;
struct Mesh {
	Vec3 *vert;
	u32 vert_count;

	Face* faces;
	u32 faces_count;
};

typedef struct LinearTransform LinearTransform;
struct LinearTransform {
	f32 ax, bx, cx, dx;
	f32 ay, by, cy, dy;
	f32 az, bz, cz, dz;
};

typedef struct MeshInstance MeshInstance;
struct MeshInstance {
	Mesh mesh;
	LinearTransform transform;
};

typedef struct Player Player;
struct Player {
	Vec3 pos;
	Vec3 rot;
};

typedef struct GameState GameState;
struct GameState {
	Player  player;
	MeshInstance mesh_instance;
};

#define proper_modf(a, b) (modf32((modf32(a, b) + b), b))


f32 modf32(f32 a, f32 b)
{
	i32 d = a / b;
	return a - b*d;
}

f32 linear_transform_op(f32 a, f32 b, f32 c, f32 d, Vec3 v) 
{
	return a*v.x + b*v.y + c*v.z + d;
}

LinearTransform linear_transform_mul(LinearTransform u, LinearTransform z) 
{
	return (LinearTransform){
		linear_transform_op(u.ax, u.bx, u.cx, 0.0f, (Vec3){z.ax, z.ay, z.az}),
		linear_transform_op(u.ax, u.bx, u.cx, 0.0f, (Vec3){z.bx, z.by, z.bz}),
		linear_transform_op(u.ax, u.bx, u.cx, 0.0f, (Vec3){z.cx, z.cy, z.cz}),
		linear_transform_op(u.ax, u.bx, u.cx, u.dx, (Vec3){z.dx, z.dy, z.dz}),
                                                                     
		linear_transform_op(u.ay, u.by, u.cy, 0.0f, (Vec3){z.ax, z.ay, z.az}),
		linear_transform_op(u.ay, u.by, u.cy, 0.0f, (Vec3){z.bx, z.by, z.bz}),
		linear_transform_op(u.ay, u.by, u.cy, 0.0f, (Vec3){z.cx, z.cy, z.cz}),
		linear_transform_op(u.ay, u.by, u.cy, u.dy, (Vec3){z.dx, z.dy, z.dz}),
                                                                     
		linear_transform_op(u.az, u.bz, u.cz, 0.0f, (Vec3){z.ax, z.ay, z.az}),
		linear_transform_op(u.az, u.bz, u.cz, 0.0f, (Vec3){z.bx, z.by, z.bz}),
		linear_transform_op(u.az, u.bz, u.cz, 0.0f, (Vec3){z.cx, z.cy, z.cz}),
		linear_transform_op(u.az, u.bz, u.cz, u.dz, (Vec3){z.dx, z.dy, z.dz})
	};
}

LinearTransform linear_transform_identity()
{
	return (LinearTransform){
		1.0f, 0.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f
	};
}

Vec3 linear_transform_apply(LinearTransform u, Vec3 z) 
{
	return (Vec3){
		linear_transform_op(u.ax, u.bx, u.cx, u.dx, z),
		linear_transform_op(u.ay, u.by, u.cy, u.dy, z),
		linear_transform_op(u.az, u.bz, u.cz, u.dz, z)
	};
}

MeshInstance mesh_create_instance(const Mesh mesh) {
	return (MeshInstance){mesh, linear_transform_identity()};
}

i32 shift_right_round(i64 a, i32 b) 
{
	return (a + ((1ll << b) -1)) >> b;
}

void software_rasterizer(
				  int32_t x1, int32_t x2, int32_t x3,
				  int32_t y1, int32_t y2, int32_t y3,
				  int32_t z1, int32_t z2, int32_t z3,
				  uint32_t *canvas, int32_t *zbuffer, uint32_t color) 
{
	// Ordering v1 v2 v3 in counter clockwise
	if ((y2 - y1) * (x3 - x2) - (x2 - x1) * (y3 - y2) > 0) {
		SWAP(int32_t, x2, x3); 
		SWAP(int32_t, y2, y3); 
		SWAP(int32_t, z2, z3); 
	}
	if ((z1 <= 0 || z1 > INV_NEAR_PLANE) && 
	 	(z2 <= 0 || z2 > INV_NEAR_PLANE) && 
	 	(z3 <= 0 || z3 > INV_NEAR_PLANE)) return;

	// Step functions *16
	i32 dx21 = x2 - x1;
	i32 dx32 = x3 - x2;
	i32 dx13 = x1 - x3;
	i32 dx31 = x3 - x1;

	i32 dy21 = y2 - y1;
	i32 dy32 = y3 - y2;
	i32 dy13 = y1 - y3;
	i32 dy31 = y3 - y1;

	i32 dz13 = z1 - z3;
	i32 dz23 = z2 - z3;

	// Step functions * 16 * 16
	i32 fdx21 = dx21 << 4;
	i32 fdx32 = dx32 << 4;
	i32 fdx13 = dx13 << 4;

	i32 fdy21 = dy21 << 4;
	i32 fdy32 = dy32 << 4;
	i32 fdy13 = dy13 << 4;

	// minx, miny, maxx, maxy
	i32 minxf = min3(x1, x2, x3);
	i32 minx =  max((minxf + 0xF) >> 4, 0);

	i32 minyf = min3(y1, y2, y3);
	i32 miny =  max((minyf + 0xF) >> 4, 0);
	
	i32 maxxf = max3(x1, x2, x3);
	i32 maxx =  min((maxxf + 0xF) >> 4, WINDOW_WIDTH);

	i32 maxyf = max3(y1, y2, y3);
	i32 maxy = min((maxyf + 0xF) >> 4, WINDOW_HEIGHT);

	canvas_ptr buffer = canvas + miny * WINDOW_STRIDE;
	i32 *zbufy = zbuffer + miny * WINDOW_WIDTH;

	// Equations 24.8
	i32 eq1y = dx21*((miny << 4) - y1) - dy21 * ((minx << 4) - x1);
	i32 eq2y = dx32*((miny << 4) - y2) - dy32 * ((minx << 4) - x2);
	i32 eq3y = dx13*((miny << 4) - y3) - dy13 * ((minx << 4) - x3);

	i32 area = dx21*dy31 - dy21 * dx31; // 24.8
	if (area == 0) area = 1;

	i64 fdzxt =  ((-(i64)dz13*dy32 - (i64)dz23*dy13) << 16)/area;
	i64 fdzyt =  (( (i64)dz13*dx32 + (i64)dz23*dx13) << 16)/area;

	i32 fdzx = shift_right_round(fdzxt, 12);
	i32 fdzy = shift_right_round(fdzyt, 12);

	i64 zd1 = (((i64)eq2y*dz13 + (i64)eq3y*dz23) << 16)/area;

	i32 eqz = z3 + shift_right_round(zd1, 16);

	// Correct for fill convention
    // if(dy21 < 0 || (dy21 == 0 && dx21 > 0)) eq1y++;
    // if(dy32 < 0 || (dy32 == 0 && dx32 > 0)) eq2y++;
    // if(dy13 < 0 || (dy13 == 0 && dx13 > 0)) eq3y++;

	for (i32 y = miny; y < maxy; y++) {
		canvas_ptr xbuffer = buffer + minx;
		i32 *zb =      zbufy + minx;
		i32 eq1x = eq1y;
		i32 eq2x = eq2y;
		i32 eq3x = eq3y;
		i32 eqzx = eqz;
		for (i32 x = minx; x < maxx; x++) {
			if (eq1x > 0 &&
				eq2x > 0 &&
				eq3x > 0) {
				 if (*zb < eqzx && eqzx < INV_NEAR_PLANE) {
					*zb = eqzx;
					*xbuffer = color;
				}
			}
			xbuffer++;
			zb++;
			eq1x -= fdy21;
			eq2x -= fdy32;
			eq3x -= fdy13;
			eqzx += fdzx;
		}
		buffer += WINDOW_STRIDE;
		zbufy += WINDOW_WIDTH;
		eq1y += fdx21;
		eq2y += fdx32;
		eq3y += fdx13;
		eqz += fdzy;
	}
}

Vec2 vec3_project_to_2D(Vec3 t) {
	return (Vec2){t.x / t.z, t.y / t.z};
}

Vec2 vec2_to_screen_space(Vec2 t) {
	return (Vec2){(t.x + 1.0f) / 2.0f * WINDOW_WIDTH, 
				  (t.y + 1.0f) / 2.0f * WINDOW_HEIGHT};
}


const PixelType colors[] = {GREEN};
const u32 colors_count = sizeof(colors) / sizeof(*colors);

#include "../data/triangle.c"
#include "../data/triangle2.c"
#include "../data/tetraedro.c"
#include "../data/cup.c"

void draw_mesh(GameState *game, canvas_ptr canvas, i32 *zbuffer, RasterizationFunction fn)
{
	LinearTransform translation_transform = linear_transform_identity();
	translation_transform.dx = -game->player.pos.x;
	translation_transform.dy = -game->player.pos.y;
	translation_transform.dz = -game->player.pos.z;

	f32 tetay = game->player.rot.y;
	f32 tetax = game->player.rot.x;

	LinearTransform rotationy_transform = {
		cosf(tetay),  0.0f, sinf(tetay), 0.0f,
		0.0f,         1.0f, 0.0f,        0.0f,
		-sinf(tetay), 0.0f, cosf(tetay), 0.0f
	};

	LinearTransform rotationx_transform = {
		1.0f, 0.0f,        0.0f,         0.0f,
		0.0f, cosf(tetax), -sinf(tetax), 0.0f,
		0.0f, sinf(tetax), cosf(tetax),  0.0f
	};

	LinearTransform player_transform = linear_transform_mul(
		translation_transform,
		linear_transform_mul(rotationy_transform, rotationx_transform)
	);

	MeshInstance m = game->mesh_instance;
	Vec3 *v = m.mesh.vert;

	for (u32 i = 0; i < m.mesh.faces_count; i++) {
		Face f = m.mesh.faces[i];
		
		LinearTransform transform = linear_transform_mul(m.transform, player_transform);

		Vec3 tv1 = linear_transform_apply(transform, v[f.v1]);
		Vec3 tv2 = linear_transform_apply(transform, v[f.v2]);
		Vec3 tv3 = linear_transform_apply(transform, v[f.v3]);


		Vec2 pv1 = vec2_to_screen_space(vec3_project_to_2D(tv1));
		Vec2 pv2 = vec2_to_screen_space(vec3_project_to_2D(tv2));
		Vec2 pv3 = vec2_to_screen_space(vec3_project_to_2D(tv3));

		int32_t x1 = pv1.x * 16.0f;
		int32_t x2 = pv2.x * 16.0f;
		int32_t x3 = pv3.x * 16.0f;

		int32_t y1 = pv1.y * 16.0f;
		int32_t y2 = pv2.y * 16.0f;
		int32_t y3 = pv3.y * 16.0f;

		int32_t iz1 = 1.0f/tv1.z * (1 << Z_FRACTION_BITS);
		int32_t iz2 = 1.0f/tv2.z * (1 << Z_FRACTION_BITS);
		int32_t iz3 = 1.0f/tv3.z * (1 << Z_FRACTION_BITS);

		fn(
				  x1,  x2, x3,
				  y1,  y2, y3,
				  iz1, iz2, iz3,
				  canvas, zbuffer, colors[i%colors_count]);

	}
}

u32 circuit_canvas[WINDOW_WIDTH*WINDOW_HEIGHT];
i32 circuit_zbuffer[WINDOW_WIDTH*WINDOW_HEIGHT];

u32 software_canvas[WINDOW_WIDTH*WINDOW_HEIGHT];
i32 software_zbuffer[WINDOW_WIDTH*WINDOW_HEIGHT];

void test_rasterizer(GameState *game) 
{
	for (int i = 0; i < WINDOW_HEIGHT; i++)  {
		for (int j = 0; j < WINDOW_WIDTH; j++) {
			circuit_canvas[i*WINDOW_STRIDE + j] = WHITE;
			software_canvas[i*WINDOW_STRIDE + j] = WHITE;
		}
	}

	for (int i= 0; i < WINDOW_WIDTH*WINDOW_HEIGHT; i++) {
		circuit_zbuffer[i] = INV_FAR_PLANE;
		software_zbuffer[i] = INV_FAR_PLANE;
	}
	draw_mesh(game, circuit_canvas, circuit_zbuffer, circuit_draw);
	draw_mesh(game, software_canvas, software_zbuffer, software_rasterizer);

	assert(memcmp(circuit_canvas, software_canvas, sizeof(circuit_canvas)) == 0);
	assert(memcmp(circuit_zbuffer, software_zbuffer, sizeof(circuit_zbuffer)) == 0);
}

f32 rand_normalized_f32()
{
	return (f32)rand() / RAND_MAX;
}

void game_init(GameState *game) 
{
	memset(&game->player, 0, sizeof(game->player));
	game->player.pos.z = -3.0f;
	game->player.pos.x = 0.0f;
	game->player.rot.y = rand_normalized_f32();
	game->mesh_instance = mesh_create_instance(cup_mesh);
}

GameState game;

i32 main(int argc, char **argv)
{
	srand(time(NULL));

	Verilated::commandArgs(argc, argv);
	tb->o_done = 1;
	game_init(&game);
	test_rasterizer(&game);

    return 0;
}

