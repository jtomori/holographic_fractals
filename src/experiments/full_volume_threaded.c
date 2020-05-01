/*
Example showing how to fill the whole volume - in parallel, with each thread filling a partition of the volume (split along x-axis).
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
} iter_3d_thread_args;

unsigned __stdcall iter_3d_threaded(void *args);

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

	// Threads count
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int threads = max(1, sysinfo.dwNumberOfProcessors - 1);

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
		voxel_size.x = (2.0f * vw.aspx) / vw.xdim * 5.0f; // TODO: Expose those multipliers to program arguments
		voxel_size.y = (2.0f * vw.aspy) / vw.xdim * 5.0f;
		voxel_size.z = (2.0f * vw.aspz) / 200.0f * 2.0f;

		HANDLE thread_handles[threads];
		iter_3d_thread_args thread_args[threads];

		float x_section = (2.0f * vw.aspx) / threads;
		for (int i = 0; i < threads; i++)
		{
			thread_args[i].start = -vw.aspx + i * x_section;
			thread_args[i].end = thread_args[i].start + x_section;
			thread_args[i].vf = &vf;
			thread_args[i].voxel_size = voxel_size;

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
				voxie_drawvox(args->vf, p_world.x, p_world.y, p_world.z, 0xffffff);
			}
		}
	}
	// printf("Finished!");
	_endthreadex(0);
	return 0;
}
