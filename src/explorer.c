/*
Fractal / implict shape explorer

Controls:
	+ / - : Increase / decrease voxels resolution. Tune it according to the performance.
	Enter : Cycle shapes

Sources:
	SDF functions: https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
*/

#include <stdlib.h>
#include <math.h>
#include <process.h>

#include "voxiebox.h"

typedef struct
{
	float start;
	float end;
	voxie_frame_t *vf;
	point3d voxel_size;
	int func_idx;
} iter_3d_thread_args;

unsigned __stdcall iter_3d_threaded(void *args);

static voxie_wind_t vw;

// Shape drawing funcs
#define DRAW_FUNCS_COUNT 3
void draw_sphere(voxie_frame_t *vf, point3d p);
void draw_torus(voxie_frame_t *vf, point3d p);
void draw_box(voxie_frame_t *vf, point3d p);

void (*draw_funcs[DRAW_FUNCS_COUNT])(voxie_frame_t *, point3d) = {
	draw_sphere,
	draw_torus,
	draw_box
};

// Math funcs
float length3d2(point3d p);
float length3d(point3d p);
float length2d(point2d p);
point3d abs3d(point3d p);
point3d subtract3d(point3d a, point3d b);
point3d max3d(point3d a, point3d b);

// Entry point
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	double tim = 0.0, otim, dtim;
	char key;
	point3d voxel_size;
	float x_section;
	int func_idx = 0;

	if (voxie_load(&vw) < 0)
		return -1;

    // Default settings
    vw.usecol = 1; // Color rendering
	vw.useemu = 2; // Simulation
	float quality = 1.0f;

	// Threads count
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int threads = max(1, sysinfo.dwNumberOfProcessors - 1);

	HANDLE thread_handles[threads];
	iter_3d_thread_args thread_args[threads];

	if (voxie_init(&vw) < 0)
		return -1;

	// Frames loop
	while (!voxie_breath(&in))
	{
		otim = tim;
		tim = voxie_klock();
		dtim = tim-otim;

		// Key presses
		// printf("%d %x\n", key, key);

		if (voxie_keystat(0x1))
			voxie_quitloop(); // ESC quit

		key = (char)voxie_keyread();
		if (key == 0x2d) // -
			quality += 0.25f;
		if (key == 0x2b) // +
			quality -= 0.25f;
		
		quality = fmaxf(fminf(quality, 3.0f), 0.0f);

		if (key == 0xd) // Enter
			func_idx++;
		
		func_idx %= DRAW_FUNCS_COUNT;

		// Start frame
		voxie_frame_start(&vf);
		voxie_setview(&vf, -vw.aspx, -vw.aspy, -vw.aspz, +vw.aspx, +vw.aspy, +vw.aspz);

		// Draw volume bbox
		voxie_drawbox(&vf, -vw.aspx, -vw.aspy, -vw.aspz, +vw.aspx, +vw.aspy, +vw.aspz, 1, 0xffffff);

		// Voxel spacing
		voxel_size.x = (2.0f * vw.aspx) / vw.xdim * (1.0f + quality * 4.0f);
		voxel_size.y = (2.0f * vw.aspy) / vw.xdim * (1.0f + quality * 4.0f);
		voxel_size.z = (2.0f * vw.aspz) / 200.0f * (1.0f + quality);

		// Start threads
		x_section = (2.0f * vw.aspx) / threads;
		for (int i = 0; i < threads; i++)
		{
			thread_args[i].start = -vw.aspx + i * x_section;
			thread_args[i].end = thread_args[i].start + x_section;
			thread_args[i].vf = &vf;
			thread_args[i].voxel_size = voxel_size;
			thread_args[i].func_idx = func_idx;

			thread_handles[i] = (HANDLE)_beginthreadex(NULL, 0, &iter_3d_threaded, &thread_args[i], 0, NULL);
		}

		// Wait for threads to finish up
		WaitForMultipleObjects(threads, thread_handles, TRUE, INFINITE);

		for (int i = 0; i < threads; i++)
		{
			CloseHandle(thread_handles[i]);
		}

		voxie_frame_end();
		voxie_getvw(&vw);
	}

	voxie_uninit(0); // Close window and unload voxiebox.dll
	return 0;
}

unsigned __stdcall iter_3d_threaded(void *pargs)
{
	iter_3d_thread_args *args = (iter_3d_thread_args *)pargs;
	
	point3d p_world;
	for (p_world.x = args->start; p_world.x < args->end; p_world.x += args->voxel_size.x)
	{
		for (p_world.y = -vw.aspy; p_world.y < vw.aspy; p_world.y += args->voxel_size.y)
		{
			for (p_world.z = -vw.aspz; p_world.z < vw.aspz; p_world.z += args->voxel_size.z)
			{
				draw_funcs[args->func_idx](args->vf, p_world);
			}
		}
	}
	_endthreadex(0);
	return 0;
}

// Shape funcs
void draw_sphere(voxie_frame_t *vf, point3d p)
{
	if (length3d(p) <= 0.3f)
		voxie_drawvox(vf, p.x, p.y, p.z, 0x00ff00);
}

void draw_torus(voxie_frame_t *vf, point3d p)
{
	point2d t = {.x = 0.2f, .y = 0.1f};
	point2d pxz = {.x = p.x, .y = p.z};
	point2d q = {.x = length2d(pxz) - t.x, .y = p.y};

	if (length2d(q) - t.y <= 0.0f)
		voxie_drawvox(vf, p.x, p.y, p.z, 0x00ff00);
}

void draw_box(voxie_frame_t *vf, point3d p)
{
	float r = 0.05f;
	point3d b = {.x = 0.3f, .y = 0.4f, .z = 0.1f};
	point3d q = subtract3d(abs3d(p), b);
	
	float sdf = length3d(max3d(q, (point3d){.x = 0.0f, .y = 0.0f, .z = 0.0f})) + fminf(fmaxf(q.x, fmaxf(q.y, q.z)), 0.0f) - r;
	if (sdf <= 0.0f)
		voxie_drawvox(vf, p.x, p.y, p.z, 0x00ff00);
}

// Math funcs
float length3d2(point3d p)
{
	return p.x*p.x + p.y*p.y + p.z*p.z;
}

float length3d(point3d p)
{
	return sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
}

float length2d(point2d p)
{
	return sqrt(p.x*p.x + p.y*p.y);
}

point3d abs3d(point3d p)
{
	return (point3d){.x = fabsf(p.x), .y = fabsf(p.y), .z = fabsf(p.z)}; 
}

point3d subtract3d(point3d a, point3d b)
{
	return (point3d){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

point3d max3d(point3d a, point3d b)
{
	return (point3d){.x = fmaxf(a.x, b.x), .y = fmaxf(a.y, b.y), .z = fmaxf(a.z, b.z)};
}