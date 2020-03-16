#include "voxiebox.h"
#include <stdlib.h>
#include <math.h>

static voxie_wind_t vw;

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	voxie_frame_t vf;
	voxie_inputs_t in;
	double tim = 0.0, otim, dtim;

	if (voxie_load(&vw) < 0) return(-1);

    // Default settings
    vw.usecol = 1; // Color rendering

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

		voxie_frame_end();
		voxie_getvw(&vw);
	}

	voxie_uninit(0); // Close window and unload voxiebox.dll
	return 0;
}
