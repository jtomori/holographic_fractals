/*
Filling the whole volume
*/

#include "voxiebox.h"
#include <stdlib.h>
#include <math.h>

void draw(voxie_frame_t *vf, point3d p);
float length(point3d p);

static voxie_wind_t vw;

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	double tim = 0.0, otim, dtim;

	if (voxie_load(&vw) < 0) return(-1);

    // Default settings
    vw.usecol = 1; // Color rendering
	vw.useemu = 2; // Simulation

	if (voxie_init(&vw) < 0) return(-1);

	while (!voxie_breath(&in))
	{
		otim = tim; tim = voxie_klock(); dtim = tim-otim;

		if (voxie_keystat(0x1)) { voxie_quitloop(); } // ESC quit

		voxie_frame_start(&vf);
		voxie_setview(&vf,-vw.aspx,-vw.aspy,-vw.aspz,+vw.aspx,+vw.aspy,+vw.aspz);

		// Draw wireframe box around volume
		voxie_drawbox(&vf,-vw.aspx,-vw.aspy,-vw.aspz,+vw.aspx,+vw.aspy,+vw.aspz,1,0xffffff);

		// Do stuff here
		point3d voxel_size;
		voxel_size.x = (2.0f * vw.aspx) / vw.xdim * 5.0f;
		voxel_size.y = (2.0f * vw.aspy) / vw.xdim * 5.0f;
		voxel_size.z = (2.0f * vw.aspz) / 200.0f * 2.0f;

		point3d p_world;
		for (p_world.x = -vw.aspx; p_world.x < vw.aspx; p_world.x += voxel_size.x)
		{
			for (p_world.y = -vw.aspy; p_world.y < vw.aspy; p_world.y += voxel_size.y)
			{
				for (p_world.z = -vw.aspz; p_world.z < vw.aspz; p_world.z += voxel_size.z)
				{
					draw(&vf, p_world);
				}
			}
		}

		voxie_frame_end();
		voxie_getvw(&vw);
	}

	voxie_uninit(0); // Close window and unload voxiebox.dll
	return 0;
}

void draw(voxie_frame_t *vf, point3d p)
{
	// if (length(p) <= 0.35f) // Sphere
		voxie_drawvox(vf, p.x, p.y, p.z, 0xffffff);
}

float length(point3d p)
{
	return sqrtf(p.x*p.x + p.y*p.y + p.z*p.z);
}