#if 0
fractals_ben.exe: fractals_ben.c voxiebox.h; cl /TP fractals_ben.c /Ox /MT /link user32.lib
	del fractals_ben.obj
#fractals.exe : fractals.c; \Dev - Cpp\bin\gcc fractals.c - o fractals.exe - pipe - O3 - s
!if 0
#endif

/*
Holographic fractals explorer by Ben Weatherall

Controls:
	+ / - : Increase / decrease voxels resolution. Tune it according to the performance.
	Enter : Cycle shapes
	PGUP/PGDN, Spacenav buts: scale
	Spacenav: modify pos&ori
	Shift/Ctrl + [/] etc : Adjust emulation view

Sources:
	SDF functions: https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
*/

#include <stdlib.h>
#include <math.h>
#include <process.h>

#include "voxiebox.h"

typedef struct
{
	float start, end;
	voxie_frame_t *vf;
	point3d voxel_size, pp, pr, pd, pf;
	int func_idx;
} iter_3d_thread_args;

unsigned __stdcall iter_3d_threaded(void *args);

static voxie_wind_t vw;

// Shape drawing funcs
#define DRAW_FUNCS_COUNT 5
int draw_sphere(voxie_frame_t *vf, point3d p);
int draw_torus(voxie_frame_t *vf, point3d p);
int draw_box(voxie_frame_t *vf, point3d p);
int draw_mandelbulb(voxie_frame_t *vf, point3d p);
int draw_bristorbrot(voxie_frame_t *vf, point3d p);

// Fractals
void mandelbulb_iter(point3d *z, point3d p_in, float power);
void bristorbrot_iter(point3d *z, point3d p_in);

int (*draw_funcs[DRAW_FUNCS_COUNT])(voxie_frame_t *, point3d) = {
	draw_sphere,
	draw_bristorbrot,
	draw_mandelbulb,
	draw_torus,
	draw_box};

// Math funcs
float length3d2(point3d p);
float length3d(point3d p);
float length2d(point2d p);
point3d abs3d(point3d p);
point3d add3d(point3d a, point3d b);
point3d subtract3d(point3d a, point3d b);
point3d max3d(point3d a, point3d b);

#define PI 3.14159265358979323

//Rotate vectors a & b around their common plane, by ang
static void rotate_vex(float ang, point3d *a, point3d *b)
{
	float f, c, s;
	int i;

	c = cos(ang);
	s = sin(ang);
	f = a->x;
	a->x = f * c + b->x * s;
	b->x = b->x * c - f * s;
	f = a->y;
	a->y = f * c + b->y * s;
	b->y = b->y * c - f * s;
	f = a->z;
	a->z = f * c + b->z * s;
	b->z = b->z * c - f * s;
}

// Entry point
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	double tim = 0.0, otim, dtim;
	char key;
	point3d voxel_size, pp = {0.f, 0.f, 0.f}, pr = {1.f, 0.f, 0.f}, pd = {0.f, 1.f, 0.f}, pf = {0.f, 0.f, 1.f};
	float x_section, scale = 1.f;
	int i, func_idx = 0;
	voxie_nav_t nav[4] = {0};
	int onavbut[4];

	if (voxie_load(&vw) < 0)
		return -1;

	// Default settings
	vw.usecol = 1; // Color rendering
	vw.useemu = 1; // Simulation
	float quality = 1.0f;

	// Threads count
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int threads = max(1, sysinfo.dwNumberOfProcessors - 1);

	HANDLE thread_handles[16];			 //threads];
	iter_3d_thread_args thread_args[16]; //threads];

	if (voxie_init(&vw) < 0)
		return -1;

	// Frames loop
	while (!voxie_breath(&in))
	{
		otim = tim;
		tim = voxie_klock();
		dtim = tim - otim;

		i = (voxie_keystat(0x1b) & 1) - (voxie_keystat(0x1a) & 1);
		if (i)
		{
			if (voxie_keystat(0x2a) | voxie_keystat(0x36))
				vw.emuvang = min(max(vw.emuvang + (float)i * dtim * 2.0, -PI * .5), 0.1268); //Shift+[,]
			else if (voxie_keystat(0x1d) | voxie_keystat(0x9d))
				vw.emudist = max(vw.emudist - (float)i * dtim * 2048.0, 400.0); //Ctrl+[,]
			else
				vw.emuhang += (float)i * dtim * 2.0; //[,]
			voxie_init(&vw);
		}

		for (i = 0; i < 4; i++)
		{
			onavbut[i] = nav[i].but;
			voxie_nav_read(i, &nav[i]);
		}

		// Key presses
		// printf("%d %x\n", key, key);

		if (voxie_keystat(0x1))
			voxie_quitloop(); // ESC quit

		key = (char)voxie_keyread();
		if (key == 0x2d)
			quality += 0.25f; // -
		if (key == 0x2b)
			quality -= 0.25f; // +

		if (voxie_keystat(0xd1) || (nav[0].but & 1))
			scale *= pow(1.5, dtim); //PGDN
		if (voxie_keystat(0xc9) || (nav[0].but & 2))
			scale /= pow(1.5, dtim); //PGUP
		float fx, fy, fz;
		fx = nav[0].dx * dtim * -.001f;
		fy = nav[0].dy * dtim * -.001f;
		fz = nav[0].dz * dtim * -.001f;
		pp.x += fx * pr.x + fy * pr.y + fz * pr.z;
		pp.y += fx * pd.x + fy * pd.y + fz * pd.z;
		pp.z += fx * pf.x + fy * pf.y + fz * pf.z;

		//Hacks for nice matrix movement with Space Navigator
		float f;
		f = pr.y;
		pr.y = pd.x;
		pd.x = f;
		f = pr.z;
		pr.z = pf.x;
		pf.x = f;
		f = pd.z;
		pd.z = pf.y;
		pf.y = f;
		rotate_vex(nav[0].az * dtim * -.005f, &pr, &pd);
		rotate_vex(nav[0].ay * dtim * -.005f, &pd, &pf);
		rotate_vex(nav[0].ax * dtim * +.005f, &pf, &pr);
		f = pr.y;
		pr.y = pd.x;
		pd.x = f;
		f = pr.z;
		pr.z = pf.x;
		pf.x = f;
		f = pd.z;
		pd.z = pf.y;
		pf.y = f;

		quality = fmaxf(fminf(quality, 3.0f), 0.0f);

		if (key == 0xd)
			func_idx++; // Enter

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
		for (i = 0; i < threads; i++)
		{
			thread_args[i].start = -vw.aspx + i * x_section;
			thread_args[i].end = thread_args[i].start + x_section;
			thread_args[i].vf = &vf;
			thread_args[i].voxel_size = voxel_size;
			thread_args[i].func_idx = func_idx;
			thread_args[i].pp = pp;
			thread_args[i].pr.x = pr.x * scale;
			thread_args[i].pr.y = pr.y * scale;
			thread_args[i].pr.z = pr.z * scale;
			thread_args[i].pd.x = pd.x * scale;
			thread_args[i].pd.y = pd.y * scale;
			thread_args[i].pd.z = pd.z * scale;
			thread_args[i].pf.x = pf.x * scale;
			thread_args[i].pf.y = pf.y * scale;
			thread_args[i].pf.z = pf.z * scale;

			thread_handles[i] = (HANDLE)_beginthreadex(NULL, 0, &iter_3d_threaded, &thread_args[i], 0, NULL);
		}

		// Wait for threads to finish up
		WaitForMultipleObjects(threads, thread_handles, TRUE, INFINITE);

		for (i = 0; i < threads; i++)
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
	poltex_t vt[4096];
	int vtn = 0;
	for (p_world.x = args->start; p_world.x < args->end; p_world.x += args->voxel_size.x)
	{
		for (p_world.y = -vw.aspy; p_world.y < vw.aspy; p_world.y += args->voxel_size.y)
		{
			for (p_world.z = -vw.aspz; p_world.z < vw.aspz; p_world.z += args->voxel_size.z)
			{
				if (draw_funcs[args->func_idx](args->vf, p_world))
				{
					float fx, fy, fz;
					fx = p_world.x - args->pp.x;
					fy = p_world.y - args->pp.y;
					fz = p_world.z - args->pp.z;
					vt[vtn].x = fx * args->pr.x + fy * args->pd.x + fz * args->pf.x;
					vt[vtn].y = fx * args->pr.y + fy * args->pd.y + fz * args->pf.y;
					vt[vtn].z = fx * args->pr.z + fy * args->pd.z + fz * args->pf.z;
					vt[vtn].col = 0xffffff;
					vtn++;
					if (vtn >= sizeof(vt) / sizeof(vt[0]))
					{
						voxie_drawmeshtex(args->vf, 0, vt, vtn, 0, 0, 0, 0xffffff);
						vtn = 0;
					}
				}
			}
		}
	}
	if (vtn)
	{
		voxie_drawmeshtex(args->vf, 0, vt, vtn, 0, 0, 0, 0xffffff);
	}

	_endthreadex(0);
	return 0;
}

// Shape funcs
int draw_sphere(voxie_frame_t *vf, point3d p)
{
	return (length3d(p) <= 0.3f);
}

int draw_torus(voxie_frame_t *vf, point3d p)
{
	point2d t = {0.2f, 0.1f};
	point2d pxz = {p.x, p.z};
	point2d q = {length2d(pxz) - t.x, p.y};

	return (length2d(q) - t.y <= 0.0f);
}

int draw_box(voxie_frame_t *vf, point3d p)
{
	float r = 0.05f;
	point3d b = {0.3f, 0.4f, 0.1f};
	point3d q = subtract3d(abs3d(p), b);

	point3d s = {0.f, 0.f, 0.f};
	float sdf = length3d(max3d(q, s)) + fminf(fmaxf(q.x, fmaxf(q.y, q.z)), 0.0f) - r;
	return (sdf <= 0.0f);
}

int draw_mandelbulb(voxie_frame_t *vf, point3d p)
{
	int max_iterations = 5;
	int max_distance = 20;

	point3d p_in = p;
	point3d z = p_in;
	float z_dist = length3d2(z);

	int i = 0;
	for (; i < max_iterations; i++)
	{
		mandelbulb_iter(&z, p_in, 8.0f);

		z_dist = length3d2(z);
		if (z_dist > max_distance)
			break;
	}

	return (i == max_iterations);
}

int draw_bristorbrot(voxie_frame_t *vf, point3d p)
{
	int max_iterations = 30;
	int max_distance = 200;

	point3d p_in = p;
	point3d z = p_in;
	float z_dist = length3d2(z);

	int i = 0;
	for (; i < max_iterations; i++)
	{
		bristorbrot_iter(&z, p_in);

		z_dist = length3d2(z);
		if (z_dist > max_distance)
			break;
	}

	return (i == max_iterations);
}

void mandelbulb_iter(point3d *z, point3d p_in, float power)
{
	point3d z_orig = *z;

	float distance = length3d(*z);

	// convert to polar coordinates
	float theta = acosf(z->z / distance);
	float phi = atan2f(z->y, z->x);

	// scale and rotate the point
	float zr = powf(distance, power);
	theta *= power;
	phi *= power;

	// convert back to cartesian coordinates
	point3d new_p = {sinf(theta) * cosf(phi) * zr, sinf(phi) * sinf(theta) * zr, cosf(theta) * zr};

	*z = add3d(new_p, p_in);
}

void bristorbrot_iter(point3d *z, point3d p_in)
{
	point3d z_orig = *z;

	point3d new_p;
	new_p.x = z->x * z->x - z->y * z->y - z->z * z->z;
	new_p.y = z->y * (2.0f * z->x - z->z);
	new_p.z = z->z * (2.0f * z->x + z->y);

	*z = add3d(new_p, p_in);
}

// Math funcs
float length3d2(point3d p)
{
	return p.x * p.x + p.y * p.y + p.z * p.z;
}

float length3d(point3d p)
{
	return sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

float length2d(point2d p)
{
	return sqrt(p.x * p.x + p.y * p.y);
}

point3d abs3d(point3d p)
{
	point3d q = {fabsf(p.x), fabsf(p.y), fabsf(p.z)};
	return q;
}

point3d add3d(point3d a, point3d b)
{
	point3d q = {a.x + b.x, a.y + b.y, a.z + b.z};
	return q;
}

point3d subtract3d(point3d a, point3d b)
{
	point3d q = {a.x - b.x, a.y - b.y, a.z - b.z};
	return q;
}

point3d max3d(point3d a, point3d b)
{
	point3d q = {fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)};
	return q;
}

#if 0
!endif
#endif
