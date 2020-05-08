#if 0
fractals_ken.exe: fractals_ken.c voxiebox.h; cl /TP fractals_ken.c /Ox /GFy /MT /link user32.lib
	del fractals_ken.obj
#fractals.exe : fractals.c; \Dev - Cpp\bin\gcc fractals.c - o fractals.exe - pipe - O3 - s
!if 0
#endif

/*
Holographic fractal explorer
	Started by Juraj Tomori (https://github.com/jtomori)
	Heavily modified & optimized by Ken Silverman

Controls:
	+ -               Adjust target volume rate (fps). Affects
	PGUP PGDN         Select shape
	, .               Select palette (applicable to fractals)
	A Z               Scale object
	SpaceNav buts     Scale object
	SpaceNav          Scroll pos&ori
	Arrows RCTRL KP0  Scroll pos
	/ Space           Reset pos&ori
	Shift/Ctrl + [/]  Adjust emulation viewpoint/size

Sources:
	SDF functions: https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
*/

#include <stdlib.h>
#include <math.h>
#include <intrin.h>
#include <process.h>
#include "voxiebox.h"
static voxie_wind_t vw;
#define PI 3.14159265358979323

static double gtim;

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

//Obsolete helper math..
//float vleng2 (point2d p) { return p.x*p.x + p.y*p.y; }
//float vleng2 (point3d p) { return p.x*p.x + p.y*p.y + p.z*p.z; }
//point3d vabs (point3d p) { point3d q = {fabsf(p.x), fabsf(p.y), fabsf(p.z)}; return q; }
//point3d vmax (point3d a, point3d b) { point3d q = {fmaxf(a.x,b.x), fmaxf(a.y,b.y), fmaxf(a.z,b.z)}; return q; }
//point3d vadd (point3d a, point3d b) { point3d q = {a.x+b.x, a.y+b.y, a.z+b.z}; return q; }
//point3d vsub (point3d a, point3d b) { point3d q = {a.x-b.x, a.y-b.y, a.z-b.z}; return q; }

//Useful helper math ;)
#if 1
#define rcpss(f) _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(f)))
#define rsqrtss(f) _mm_cvtss_f32(_mm_rsqrt_ps(_mm_set_ss(f)))
#else
#define rcpss(f) (1.0 / f)			//use in case above doesn't compile
#define rsqrtss(f) (1.0 / sqrtf(f)) //use in case above doesn't compile
#endif

static int palcur[8];
static const int paltab[][sizeof(palcur) / sizeof(palcur[0])] =
	{
		0x000000,
		0x0000ff,
		0xff0000,
		0x00ff00,
		0x000000,
		0x0000ff,
		0xff0000,
		0x00ff00,
		0x000000,
		0x0000ff,
		0xff0000,
		0x00ff00,
		0xff00ff,
		0x00ffff,
		0xffff00,
		0xffffff,
		0xffffff,
		0xffff00,
		0x00ffff,
		0xff00ff,
		0x00ff00,
		0xff0000,
		0x0000ff,
		0x000000,
};

int rgb(float r, float g, float b)
{
	return min(max((int)r, 0), 255) * 65536 +
		   min(max((int)g, 0), 255) * 256 +
		   min(max((int)b, 0), 255);
}

//shape funcs..

int getvox_mandelbulb(point3d p)
{
	point3d w;
	float x, y, z, x2, y2, z2, x4, y4, z4, k1, k2, k3, k4;
	int i;

	w = p; //Based on fast algo here: https://www.iquilezles.org/www/articles/mandelbulb/mandelbulb.htm
	for (i = sizeof(palcur) / sizeof(palcur[0]) - 1; i > 0; i--)
	{
		x = w.x;
		x2 = x * x;
		x4 = x2 * x2;
		y = w.y;
		y2 = y * y;
		y4 = y2 * y2;
		z = w.z;
		z2 = z * z;
		z4 = z2 * z2;

		k1 = x4 + y4 + z4 - (x2 + z2) * y2 * 6.f + x2 * z2 * 2.f;
		k3 = x2 + z2;
		k4 = k3 - y2;
		k2 = k3 * k3 * k3;
		k2 = rsqrtss(k2 * k2 * k3) * k1 * k4 * y;

		w.x = (x2 * z2 * 6.f - x4 - z4) * (z2 - x2) * x * z * k2 * 64.f;
		w.y = k1 * k1 - y2 * k3 * k4 * k4 * 16.f;
		w.z = ((x4 * 70.f - x2 * z2 * 28.f + z4) * z4 + (x4 - x2 * z2 * 28.f) * x4) * k2 * -8.f;

		w.x += p.x;
		w.y += p.y;
		w.z += p.z;
		if (w.x * w.x + w.y * w.y + w.z * w.z > 20.f)
			break;
	}
	return palcur[i];
}

static void mul_quat(float s1, float a1, float b1, float c1, float s2, float a2, float b2, float c2, float *s3, float *a3, float *b3, float *c3)
{
	*s3 = s1 * s2 - a1 * a2 - b1 * b2 - c1 * c2;
	*a3 = b1 * c2 - c1 * b2 + s1 * a2 + s2 * a1;
	*b3 = c1 * a2 - a1 * c2 + s1 * b2 + s2 * b1;
	*c3 = a1 * b2 - b1 * a2 + s1 * c2 + s2 * c1;
}
int getvox_quatmat_rabbit(point3d p)
{
	//3D fractal ("rabbit") by Peter Houska, (c) 2007
	//http://stud3.tuwien.ac.at/~e9907459
	float x, y, z, r, g, v, iter, iter_dec, in_s, in_a, in_b, in_c, s3, a3, b3, c3, s, a, b, c, lambda_s, lambda_a, lambda_b, lambda_c, m_s, m_a, m_b, m_c;

	x = p.x;
	y = p.y;
	z = p.z;

	iter = 1;
	iter_dec = 1.0 / 32; //smaller values for iter_dec => more accurate set-boundary approximation
						 //quaternion q is represented by: q = s + i*a + j*b + k*c
	in_s = x * 1.25 + 0.5;
	in_a = y * 1.25;
	in_b = z * 1.25;
	in_c = 0.0;
	s3 = in_s;
	a3 = in_a;
	b3 = in_b;
	c3 = in_c;
	s = in_s;
	a = in_a;
	b = in_b;
	c = in_c;
	lambda_s = -0.57;
	lambda_a = 1.0;
	lambda_b = 0.0;
	lambda_c = 0.0;
	m_s = 0.0;
	m_a = 0.0;
	m_b = 0.0;
	m_c = 0.0;
	do
	{
		//self-squaring iteration-function: q=lambda*q*(1-q)
		mul_quat(lambda_s, lambda_a, lambda_b, lambda_c, s, a, b, c, &s3, &a3, &b3, &c3);
		m_s = 1 - s;
		m_a = -a;
		m_b = -b;
		m_c = -c;
		mul_quat(s3, a3, b3, c3, m_s, m_a, m_b, m_c, &s, &a, &b, &c);
		if (s * s + a * a + b * b + c * c >= 4)
			return (0); //distance-test: if already too far away => air voxel
		iter -= iter_dec;
	} while (iter > 0);
	v = 0.7 * sqrt(x * x + y * y + z * z); //"spherical" color scheme
	r = exp(((v - .75) * (v - .75)) * (-8)) * 255;
	g = exp(((v - .50) * (v - .50)) * (-8)) * 255;
	b = exp(((v - .25) * (v - .25)) * (-8)) * 255;
	return (rgb(r, g, b));
}

int getvox_bristorbrot(point3d p)
{
	point3d z = p, oz;
	int i;

	for (i = 16; i > 0; i--)
	{
		oz = z;
		z.x = oz.x * oz.x - oz.y * oz.y - oz.z * oz.z + p.x;
		z.y = (oz.x * 2.f - oz.z) * oz.y + p.y;
		z.z = (oz.x * 2.f + oz.y) * oz.z + p.z;
		if (z.x * z.x + z.y * z.y + z.z * z.z > 200.f)
			break; //return 0;
	}
	return palcur[i & 7];
}

int getvox_tomwaves(point3d p) //by Tomasz Dobrowolski, 07/19/2014, originally for Graphcalc
{
	float co, si, px, py, pz, fx, fy, fz, d1, d2, d3;
	co = cos(gtim * .5);
	si = sin(gtim * .425);
	px = p.x * p.x * 1.5 - .50;
	py = p.y * p.y * 1.5 - .50;
	pz = p.z * p.z * 4.0 - .25;
	fx = px + co * .5;
	fy = py - si * .5;
	fz = pz + fabs(si) * .4;
	d1 = fx * fx + fy * fy + fz * fz;
	fx = px - co * .5;
	fy = py + si * .5;
	fz = pz + fabs(co) * .4;
	d2 = fx * fx + fy * fy + fz * fz;
	fz = -p.z * 2 - co * .1;
	d3 = p.x * p.x + p.y * p.y + fz * fz * fz;
	if (rcpss(d1) + rcpss(d2) + rcpss(d3) > 20)
		return (rgb(d1 * 256, d2 * 256, d3 * 256));
	return 0;
}

int getvox_hillebrand(point3d p)
{
	float x, y, z, d, c, ca, tt, xx, yy, zz, j, r, g, b;

	x = (p.x * .5 + .5) * 128;
	y = (p.y * .5 + .5) * 128;
	z = (p.z * .5 + .5) * 128;

	d = (x - 64) * (x - 64) + (y - 64) * (y - 64) + (z - 64) * (z - 64);
	c = 0;
	ca = 128 + (int)((sin(x * .5) + cos(y * .4) + cos(z * .2)) * 16);
	if (d > 60 * 60)
		c = ca;
	else if ((x > 64 - 12) && (x < 64 + 12) && (y > 64 - 12) && (y < 64 + 12) && (z > 64 - 12) && (z < 64 + 12))
		c = ca + 32;
	else
	{
		tt = 0;
		xx = ((int)(x - 13) & 31);
		if ((xx < 2) || ((xx >= 4) && (xx < 6)))
			tt++;
		yy = ((int)(y - 13) & 31);
		if ((yy < 2) || ((yy >= 4) && (yy < 6)))
			tt++;
		zz = ((int)(z - 13) & 31);
		if ((zz < 2) || ((zz >= 4) && (zz < 6)))
			tt++;
		if (tt >= 2)
			c = ca - 32;
	}

	j = (c + 16) * (c + 16);
	r = j / (1156 / 4);
	g = j / (1256 / 4);
	b = j / (1656 / 4);
	if (c <= 0)
		return 0;
	return rgb(r, g, b);
}

int getvox_tominterf(point3d p)
{
	float x, y, z, r, g, b, fx, fx2, z2, f;

	x = p.x;
	y = p.y;
	z = p.z;

	fx = x + .5;
	fx2 = x - .5;
	z2 = (sin(fx * fx * 10 + y * y * 10 + gtim) + sin(fx2 * fx2 * 10 + y * y * 10 + gtim)) * .05;
	f = max((1 - x * x - y * y) * 2, 0);
	if (z < (z2 + .2) * f && z > (z2 - .2) * f)
	{
		r = x * 50 + 128;
		g = z * 128 + 128;
		b = y * 50 + 128;
		return rgb(r, g, b);
	}
	return 0;
}

int getvox_pacman(point3d p) //by Ken Silverman, originally for Evaldraw
{
	static float glob[4];
	float x, y, z, ox, oy, ax, ay, fx, fy, fz, ix, iy, iz, r, g, b, d, t;

	x = p.x;
	y = p.y;
	z = p.z;
	t = gtim;

	if (fabs(z) > .34)
		return (0); //early-out
	if (fabs(x * x + y * y - .554) > .45)
		return (0); //early-out

	if (glob[0] != t) //pre-calculate cos&sin
	{				  //eat counter :)
		glob[0] = t;
		t *= .7;
		glob[2] = cos(t);
		glob[3] = sin(t);
		t += .3;
		glob[1] = .5 - .5 * sin(t * 16 + cos(t * 16) * .5);
	}

	//Put dots in motion are circle
	ox = x;
	oy = y;
	x = glob[2] * ox + glob[3] * oy;
	y = glob[2] * oy - glob[3] * ox;

	ax = fabs(x);
	ay = fabs(y); //Draw 8 dots w/symmetry
	fx = (max(ax, ay) - cos(PI / 8) / sqrt(2.0));
	fy = (min(ax, ay) - sin(PI / 8) / sqrt(2.0));
	if (fx * fx + fy * fy + z * z < .06 * .06)
	{
		r = 192;
		g = 255;
		b = 255;
		return (rgb(r, g, b));
	}

	//Put Pacman in motion around circle
	x = (glob[2] * ox - glob[3] * oy) * 3 + 2;
	y = (glob[2] * oy + glob[3] * ox) * 3;
	z *= 3;

	//Render Pacman's eyes
	ix = .40;
	iy = .50;
	iz = .70;
	fx = fabs(x) - ix;
	fy = y - iy;
	fz = z + iz;
	if (fx * fx + fy * fy + fz * fz < .24 * .24)
	{
		fx = fabs(x + glob[2] * .20) - ix;
		fy = (y - iy - .10 - fabs(glob[3]) * .20);
		fz = z + iz + .10;
		if (fx * fx + fy * fy + fz * fz < .15 * .15)
			r = 0;
		else
			r = 255;
		g = r;
		b = r;
		return (rgb(r, g, b));
	}

	d = x * x + y * y + z * z;
	if (d > 1 * 1)
		return (0); //Pacman's sphere
	if (fabs(z - .1) < y * glob[1])
		return (0); //mouth

	if (d < .9 * .9)
	{
		r = 96;
		g = 96;
		b = 48;
	} //Mouth
	else
	{
		r = 192;
		g = 192;
		b = 96;
	} //Body
	return (rgb(r, g, b));
}

int getvox_julia(point3d p)
{
	float x, y, z, x2, y2, z2, fx, fy, fz, d1, d2, r, g, b;
	int i;

	//Original Julia script by Inigo Quilez
	x = p.x * 1.5;
	x2 = x * x;
	y = p.y * 1.5;
	y2 = y * y;
	z = p.z * 1.5;
	z2 = z * z;
	d1 = 1e32;
	d2 = 1e32;
	i = 9;
	do
	{
		y *= x;
		z *= x;
		x = x2 - y2 - z2 - .7;
		x2 = x * x;
		y = y * 2 + .2;
		y2 = y * y;
		z = z * 2 - .5;
		z2 = z * z;
		if (x2 + y2 + z2 > 4.f)
			return (0);
		d1 = min(d1, x2);
		i--;
	} while (i > 0);
	d1 = sqrt(d1) * 2.0;
	fx = x - .5;
	fy = y - .3;
	fz = z - .2;
	d2 = sqrt(fx * fx + fy * fy + fz * fz) * 0.3;
	r = 112 + 145 * d1 - 20 * d2;
	g = 112 + 163 * d1 - 20 * d2;
	b = 92 + 163 * d1 - 30 * d2;
	return (rgb(r, g, b));
}

int getvox_tomquat(point3d p)
{
	float x, y, z, r, g, b, mcw, mcx, mcy, mcz, cx, cy, cz, cw, _cw, _cx, _cy, _cz;
	int i, it;

	x = p.x;
	y = p.y;
	z = p.z;
	mcw = y * 0.5 - .25;
	mcx = x * 0.5 + .6;
	mcy = z * 0.5 + .6;
	mcz = 0; //t*.1-1;

	cw = z;
	cx = x;
	cy = 0;
	cz = y;

	//it = t*.2+1;
	it = 10;
	for (i = 0; i < it; i++)
	{
		// do the quaternion multiplication:
		_cw = cw * cw - cx * cx - cy * cy - cz * cz + mcw;
		_cx = cw * cx + cx * cw + cy * cz - cz * cy + mcx;
		_cy = cw * cy + cy * cw + cz * cx - cx * cz + mcy;
		_cz = cw * cz + cz * cw + cx * cy - cy * cx + mcz;
		cw = -_cw;
		cx = -_cx;
		cy = -_cy;
		cz = -_cz;
		if (cw * cw + cx * cx + cy * cy + cz * cz > 4)
			return (0); // divergent point!
	}

	r = x * 12 * it + 75;  //red
	g = y * 12 * it + 25;  //green
	b = z * 12 * it + 100; //blue
	return (rgb(r, g, b));
}

int getvox_waves(point3d p) //by Ken Silverman, originally for Graphcalc
{
	float f, a, b;
	f = p.x - .5;
	a = sin((sqrt(f * f + p.y * p.y + p.z * p.z) - gtim * .25) * 32);
	f = p.x + .5;
	b = sin((sqrt(f * f + p.y * p.y + p.z * p.z) - gtim * .25) * 32);
	return (a + b > 1.f) * 0xffffff;
	//a = (a-b)*256; return rgb(a,a,a);
}

int getvox_roundedbox(point3d p)
{
	float f;
	p.x = fmaxf(fabsf(p.x) - 0.3f, 0.f);
	p.y = fmaxf(fabsf(p.y) - 0.4f, 0.f);
	p.z = fmaxf(fabsf(p.z) - 0.1f, 0.f);
	f = .05f - fminf(fmaxf(p.x, fmaxf(p.y, p.z)), 0.f);
	return (p.x * p.x + p.y * p.y + p.z * p.z < f * f) * 0xffff00;
}
int getvox_torus(point3d p)
{
	p.x = sqrtf(p.x * p.x + p.y * p.y) - .2f;
	return (p.x * p.x + p.z * p.z <= .1f * .1f) * 0xff00ff;
}
int getvox_sphere(point3d p) { return (p.x * p.x + p.y * p.y + p.z * p.z <= 0.3f * 0.3f) * 0x00ffff; }

int (*getvox_funcs[])(point3d) =
	{
		//Ken's ratings:
		getvox_mandelbulb,	   //great
		getvox_quatmat_rabbit, //great
		getvox_bristorbrot,	   //good
		getvox_tomwaves,	   //weird & cool
		getvox_hillebrand,	   //weird but neat
		getvox_tominterf,	   //weird but cool
		getvox_pacman,		   //nice
		getvox_julia,		   //ok
		getvox_tomquat,		   //ok
		getvox_waves,		   //ok
		getvox_roundedbox,	   //boring
		getvox_torus,		   //boring
		getvox_sphere,		   //boring
};

static int gammlut[256];
typedef struct
{
	voxie_frame_t *vf;
	point3d pp, pr, pd, pf;
	int funcid, i0, i1;
} iter_3d_thread_args;
unsigned __stdcall iter_3d_thread(void *pargs)
{
	iter_3d_thread_args *args = (iter_3d_thread_args *)pargs;
	point3d p_voxbox, p_world;
	poltex_t vt[4096];
	double dx, dy, dz;
	int i, col, dp, di, vtn = 0;

	//3D dithering algo based on here:
	//http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
#define PHI3D 1.22074408460575947536

	i = args->i1 - 1;
	dx = fmod(i * (1.0 / (PHI3D)), 1.0);
	dy = fmod(i * (1.0 / (PHI3D * PHI3D)), 1.0);
	dz = fmod(i * (1.0 / (PHI3D * PHI3D * PHI3D)), 1.0);

	dp = 0x08020080;

	for (; i >= args->i0; i--)
	{
		p_voxbox.x = dx * vw.aspx * 2.f - vw.aspx;
		dx -= (1.0 / (PHI3D));
		if (dx < 0.0)
			dx += 1.0;
		p_voxbox.y = dy * vw.aspy * 2.f - vw.aspy;
		dy -= (1.0 / (PHI3D * PHI3D));
		if (dy < 0.0)
			dy += 1.0;
		p_voxbox.z = dz * vw.aspz * 2.f - vw.aspz;
		dz -= (1.0 / (PHI3D * PHI3D * PHI3D));
		if (dz < 0.0)
			dz += 1.0;

		p_world.x = p_voxbox.x * args->pr.x + p_voxbox.y * args->pd.x + p_voxbox.z * args->pf.x + args->pp.x;
		p_world.y = p_voxbox.x * args->pr.y + p_voxbox.y * args->pd.y + p_voxbox.z * args->pf.y + args->pp.y;
		p_world.z = p_voxbox.x * args->pr.z + p_voxbox.y * args->pd.z + p_voxbox.z * args->pf.z + args->pp.z;

		col = getvox_funcs[args->funcid](p_world);
#if 1
		di = (gammlut[(col >> 16) & 255] << 20) + (gammlut[(col >> 8) & 255] << 10) + gammlut[col & 255];
		dp = (dp & 0x0ff3fcff) + di;
		col = ((dp & 0x10040100) >> 8);
#endif
		if (!col)
			continue;
		vt[vtn].col = col;
		vt[vtn].x = p_voxbox.x;
		vt[vtn].y = p_voxbox.y;
		vt[vtn].z = p_voxbox.z;
		vtn++;
		if (vtn >= sizeof(vt) / sizeof(vt[0]))
		{
			voxie_drawmeshtex(args->vf, 0, vt, vtn, 0, 0, 0, 0xffffff);
			vtn = 0;
		} //buffer full - render voxel batch
	}
	if (vtn)
	{
		voxie_drawmeshtex(args->vf, 0, vt, vtn, 0, 0, 0, 0xffffff);
	} //flush (render) remaining voxels in buffer

	_endthreadex(0);
	return 0;
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	point3d pp = {0.f, 0.f, 0.f}, pr = {1.f, 0.f, 0.f}, pd = {0.f, 1.f, 0.f}, pf = {0.f, 0.f, 1.f};
	double tim = 0.0, otim, dtim;
	float f, scale = 1.f, targvps = 30.f, ogamma = -1.f;
	int i, key, funcid = 0, ndots = (1 << 21), palind = 0;
	voxie_nav_t nav[4] = {0};
	int onavbut[4];

	if (voxie_load(&vw) < 0)
		return -1;
	vw.usecol = 1; //Override color rendering
	vw.useemu = 1; //Override simulation mode
	if (voxie_init(&vw) < 0)
		return -1;

	//get thread count
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int nthreads = max(sysinfo.dwNumberOfProcessors - (!vw.useemu), 1);

	HANDLE *thread_handles = (HANDLE *)malloc(nthreads * sizeof(thread_handles[0]));
	iter_3d_thread_args *thread_args = (iter_3d_thread_args *)malloc(nthreads * sizeof(thread_args[0]));

	//main loop
	while (!voxie_breath(&in))
	{
		otim = tim;
		tim = voxie_klock();
		dtim = tim - otim;
		gtim = tim;

		while (key = voxie_keyread()) //get buffered key (low 8 bits is ASCII code, next 8 bits is keyboard scancode - for more exotic keys)
		{
			if ((char)key == 27)
			{
				voxie_quitloop();
			} //ESC:quit
			if ((char)key == '-')
			{
				targvps = fmaxf(targvps - 1.f, 3.f);
			}
			if ((char)key == '+')
			{
				targvps = fminf(targvps + 1.f, 120.f);
			}
			if ((char)key == ',')
			{
				palind--;
				if (palind < 0)
					palind = sizeof(paltab) / sizeof(palcur) - 1;
			}
			if ((char)key == '.')
			{
				palind++;
				if (palind >= sizeof(paltab) / sizeof(palcur))
					palind = 0;
			}
			if (((char)key == '/') || ((char)key == ' '))
			{
				pp = (point3d){0.f, 0.f, 0.f}, pr = (point3d){1.f, 0.f, 0.f}, pd = (point3d){0.f, 1.f, 0.f}, pf = (point3d){0.f, 0.f, 1.f};
				scale = 1.f;
			} //reset pos&ori
			if ((unsigned char)(key >> 8) == 0xc9)
			{
				funcid--;
				if (funcid < 0)
					funcid = sizeof(getvox_funcs) / sizeof(getvox_funcs[0]) - 1;
			} //PGUP
			if ((unsigned char)(key >> 8) == 0xd1)
			{
				funcid++;
				if (funcid >= sizeof(getvox_funcs) / sizeof(getvox_funcs[0]))
					funcid = 0;
			} //PGDN
		}

		for (i = 0; i < 4; i++)
		{
			onavbut[i] = nav[i].but;
			voxie_nav_read(i, &nav[i]);
		}

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

		if (voxie_keystat(0x1e) || (nav[0].but & 1))
			scale /= pow(1.5, dtim); //A
		if (voxie_keystat(0x2c) || (nav[0].but & 2))
			scale *= pow(1.5, dtim); //Z
		f = scale * dtim * .002f;
		nav[0].dx -= ((voxie_keystat(0xcb) != 0) - (voxie_keystat(0xcd) != 0)) * 256;
		nav[0].dy -= ((voxie_keystat(0xc8) != 0) - (voxie_keystat(0xd0) != 0)) * 256;
		nav[0].dz -= ((voxie_keystat(0x9d) != 0) - (voxie_keystat(0x52) != 0)) * 256;
		pp.x += (nav[0].dx * pr.x + nav[0].dy * pd.x + nav[0].dz * pf.x) * f;
		pp.y += (nav[0].dx * pr.y + nav[0].dy * pd.y + nav[0].dz * pf.y) * f;
		pp.z += (nav[0].dx * pr.x + nav[0].dy * pd.z + nav[0].dz * pf.z) * f;
		rotate_vex(nav[0].az * dtim * +.005f, &pr, &pd);
		rotate_vex(nav[0].ay * dtim * +.005f, &pd, &pf);
		rotate_vex(nav[0].ax * dtim * -.005f, &pf, &pr);

		if (vw.gamma != ogamma) //used for dithering in thread
		{
			ogamma = vw.gamma;
			for (i = 0; i < 256; i++)
			{
				gammlut[i] = (int)(pow(((float)i / 255.f), vw.gamma) * 256.f);
			}
		}

		//------------------------------------------------------------------------
		//start render
		voxie_frame_start(&vf);
		voxie_setview(&vf, -vw.aspx, -vw.aspy, -vw.aspz, +vw.aspx, +vw.aspy, +vw.aspz);

		//draw wireframe box
		voxie_drawbox(&vf, -vw.aspx, -vw.aspy, -vw.aspz, +vw.aspx, +vw.aspy, +vw.aspz, 1, 0xffffff);

		for (i = 0; i < sizeof(palcur) / sizeof(palcur[0]); i++)
		{
			palcur[i] = paltab[palind][i];
		}

		ndots += (int)((1.f / targvps - dtim) * 4194304.f); //Auto-adjust ;)
		ndots = min(max(ndots, 1 << 15), 1 << 27);

		//Start dot render threads
		int dotsperthread = ndots / nthreads;
		for (i = 0; i < nthreads; i++)
		{
			thread_args[i].i0 = (i + 0) * dotsperthread;
			thread_args[i].i1 = (i + 1) * dotsperthread;
			thread_args[i].vf = &vf;
			thread_args[i].funcid = funcid;
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
			thread_handles[i] = (HANDLE)_beginthreadex(NULL, 0, &iter_3d_thread, &thread_args[i], 0, NULL);
		}
		WaitForMultipleObjects(nthreads, thread_handles, TRUE, INFINITE); //Wait for threads to finish up
		for (i = nthreads - 1; i >= 0; i--)
		{
			CloseHandle(thread_handles[i]);
		}

		voxie_debug_print6x8_(0, 64, 0xffffff, -1, "target vps: %.1f\nactual vps: %.1f\ndots: %d\nfuncid: %d\npalind: %d", targvps, 1.f / dtim, ndots, funcid, palind);

		voxie_frame_end();
		//------------------------------------------------------------------------
		voxie_getvw(&vw); //get any menu ('\') changes
	}

	voxie_uninit(0); //close window / unload voxiebox.dll
	return 0;
}

#if 0
!endif
#endif
