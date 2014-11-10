//
//  projcl_util.h
//  Magic Maps
//
//  Created by Evan Miller on 3/31/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#pragma once

#include <CL/cl.h>
#include <projcl/projcl_types.h>
#define M_PI	         3.1415926535897932384626f
#define M_PI_2          1.570796326794896557999f
#define M_PI_4			 0.78539816339744833f
cl_kernel _pl_find_kernel(PLContext *pl_ctx, const char *requested_name);
cl_kernel _pl_find_projection_kernel(PLContext *pl_ctx, const char *name, int fwd, PLSpheroid ell);
void _pl_copy_pad(float *dest, size_t dest_count, const float *src, size_t src_count);
