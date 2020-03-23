/*
Trying threads
*/

#include "voxiebox.h"
#include <stdlib.h>
#include <math.h>
#include <process.h>

static voxie_wind_t vw;

void thread_draw_box_1(void *vf);
void thread_draw_box_2(void *vf);

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	double tim = 0.0, otim, dtim;

	if (voxie_load(&vw) < 0) return(-1);

    // Default settings
    vw.usecol = 1; // Color rendering
	vw.smear = 1;

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
		HANDLE a = (HANDLE)_beginthread(thread_draw_box_1, 0, &vf);
		HANDLE b = (HANDLE)_beginthread(thread_draw_box_2, 0, &vf);
		
		// Wait for threads
		WaitForSingleObject(a, INFINITE);
		WaitForSingleObject(b, INFINITE);

		voxie_frame_end();
		voxie_getvw(&vw);
	}

	voxie_uninit(0); // Close window and unload voxiebox.dll
	return 0;
}

void thread_draw_box_1(void *vf)
{
	voxie_frame_t *my_vf = (voxie_frame_t *)vf;
	voxie_drawbox(vf, -0.5f, -0.5f, -0.2f, 0.0f, 0.0f, 0.2f,1,0xff0000);
}

void thread_draw_box_2(void *vf)
{
	voxie_frame_t *my_vf = (voxie_frame_t *)vf;
	voxie_drawbox(vf, 0.0f, 0.0f, -0.2f, 0.5f, 0.5f, 0.2f,1,0x00ff00);
}