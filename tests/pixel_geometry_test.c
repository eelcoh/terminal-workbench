#include <assert.h>
#include "pixel_geometry.h"
int main(void)
{
    assert(pixel_scale(1240,1240)==1);assert(pixel_scale(1240,2480)==2);
    assert(pixel_scale(0,2480)==1);assert(pixel_scale(1240,0)==1);
    assert(pixel_extent(513,1)==513);assert(pixel_extent(513,2)==1026);
    assert(pixel_extent(708,2)==1416);assert(pixel_extent(10.5f,2)==21);
    assert(pixel_snap(194.25f,2)==194.5f);assert(pixel_snap(194,2)==194);
    return 0;
}
