/*
 *  projcl.c
 *  Magic Maps
 *
 *  Created by Evan Miller on 2/7/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */


#import <projcl/projcl.h>
#include <projcl/projcl_warp.h>

#include <dirent.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef WIN32 // not even sure anymore why i'm trying to compile this on windows 
typedef unsigned char           u_char;
#else 
#include <sys/uio.h> //no equivalent for windows so only include if not windows, idek man...
#endif

#include <unistd.h>
#include <sys/types.h>

#define PL_DEBUG 1

#define PL_OPENCL_FILE_EXTENSION ".opencl"
#define PL_OPENCL_KERNEL_HEADER_FILE "peel.opencl"
#define PL_OPENCL_KERNEL_FILE_PREFIX "pl_"

PLContext *pl_context_init(cl_platform_id npid, cl_device_type type, cl_int *outError) {
	cl_int error;
	cl_device_id device_id;
	
	error = clGetDeviceIDs(npid, type, 1, &device_id, NULL);
	if (error != CL_SUCCESS) {
		if (outError != NULL)
			*outError = error;
		return NULL;
	}
    
	cl_context ctx = clCreateContext(0, 1, &device_id, NULL, NULL, &error);
	if (error != CL_SUCCESS) {
		if (outError != NULL)
			*outError = error;
		return NULL;
	}
	
	cl_command_queue queue = clCreateCommandQueue(ctx, device_id,
												  0, &error);
	if (error != CL_SUCCESS) {
		clReleaseContext(ctx);
		if (outError != NULL)
			*outError = error;
		return NULL;
	}
    
#if PL_DEBUG
    int size;
    long lsize;
    //printf("OpenCL device debug info\n");
    clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(int), &size, NULL);
    //printf("-- Compute units: %d\n", size);
    clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(long), &lsize, NULL);
    //printf("-- Work group size: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(long), &lsize, NULL);
    //printf("-- Max 2D image width: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(long), &lsize, NULL);
    //printf("-- Max 2D image height: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(long), &lsize, NULL);
    //printf("-- Max 3D image width: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(long), &lsize, NULL);
    //printf("-- Max 3D image height: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(long), &lsize, NULL);
    //printf("-- Max 3D image depth: %ld\n", lsize);
    clGetDeviceInfo(device_id, CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(int), &size, NULL);
    //printf("-- Max read image args: %d\n", size);
#endif

	PLContext *pl_ctx;
	
	if ((pl_ctx = malloc(sizeof(PLContext))) == NULL) {
		clReleaseContext(ctx);
		clReleaseCommandQueue(queue);
		if (outError != NULL)
			*outError = CL_OUT_OF_HOST_MEMORY;
		return NULL;
	}
	memset(pl_ctx, 0, sizeof(PLContext));
	
	pl_ctx->ctx = ctx;
	pl_ctx->queue = queue;
	pl_ctx->device_id = device_id;
	return pl_ctx;
}

void pl_context_free(PLContext *pl_ctx) {
	clReleaseContext(pl_ctx->ctx);
	clReleaseCommandQueue(pl_ctx->queue);
	free(pl_ctx);
}

PLCode *pl_compile_code(PLContext *pl_ctx, const char *path, long modules, cl_int *outError) {
	cl_int error;
	cl_program program;
	//printf("not failing yet...\n");
	DIR *dir = opendir(path);
	if (dir == NULL) {
  //printf("ups we failed yo cus dat dir fucked up hard...\n");
        if (outError)
            *outError = 1;
		return NULL;
	}
	struct dirent *entry;
    size_t bytes_read;
	char buffer[1024*1024];
    char filename[1024];
	char * pointers[256];
	size_t buf_used = 0;
	int entry_index = 0;
    int kernel_count = 0;
    
    if (modules == 0)
        modules = ~0;
    //printf("not failing yet...\n");
    /* First read the header file */
    if (strlen(path) + sizeof(PL_OPENCL_KERNEL_HEADER_FILE) + 1 < sizeof(filename)) {
        pointers[entry_index++] = buffer + buf_used;
        sprintf(filename, "%s/" PL_OPENCL_KERNEL_HEADER_FILE, path);
        //printf("first fname: %s", filename);
        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
          //printf("Couldnt open fd???\n");
            return NULL;
        }
        while ((bytes_read = read(fd, buffer + buf_used, sizeof(buffer) - buf_used - 1)) > 0) {
            buf_used += bytes_read;
        }
        buffer[buf_used++] = '\0';
        close(fd);
    }
    
    /* Then read the routine files */
	while ((entry = readdir(dir)) != NULL) {
    //printf("WE READIN SHIT\n");
		if (entry_index >= sizeof(pointers)/sizeof(char *) - 1) {
			break;
		}
		if (buf_used >= sizeof(buffer)) {
			break;
		}
		
		size_t len = entry->d_namlen;
    //printf("len of enntry???: %d\n", len);
		const char *name = entry->d_name;
    //printf("fname: %s\n", name);
		if (len > sizeof(PL_OPENCL_FILE_EXTENSION)-1 
            && strncasecmp(name, PL_OPENCL_KERNEL_FILE_PREFIX, sizeof(PL_OPENCL_KERNEL_FILE_PREFIX) - 1) == 0
            && strcasecmp(name + len - (sizeof(PL_OPENCL_FILE_EXTENSION)-1), PL_OPENCL_FILE_EXTENSION) == 0) {
            if (strcmp(name, "pl_datum.opencl") == 0 && !(modules & PL_MODULE_DATUM))
                continue;
            if (strcmp(name, "pl_geodesic.opencl") == 0 && !(modules & PL_MODULE_GEODESIC))
                continue;
            if (strcmp(name, "pl_warp.opencl") == 0 && !(modules & PL_MODULE_WARP))
                continue;
            if (strcmp(name, "pl_sample_nearest.opencl") == 0 && !(modules & PL_MODULE_NEAREST_NEIGHBOR))
                continue;
            if (strcmp(name, "pl_sample_linear.opencl") == 0 && !(modules & PL_MODULE_BILINEAR))
                continue;
            if (strcmp(name, "pl_sample_bicubic.opencl") == 0 && !(modules & PL_MODULE_BICUBIC))
                continue;
            if (strcmp(name, "pl_sample_quasi_bicubic.opencl") == 0 && !(modules & PL_MODULE_QUASI_BICUBIC))
                continue;
            if (strcmp(name, "pl_project_albers_equal_area.opencl") == 0 && !(modules & PL_MODULE_ALBERS_EQUAL_AREA))
                continue;
            if (strcmp(name, "pl_project_american_polyconic.opencl") == 0 && !(modules & PL_MODULE_AMERICAN_POLYCONIC))
                continue;
            if (strcmp(name, "pl_project_lambert_azimuthal_equal_area.opencl") == 0 && !(modules & PL_MODULE_LAMBERT_AZIMUTHAL_EQUAL_AREA))
                continue;
            if (strcmp(name, "pl_project_lambert_conformal_conice.opencl") == 0 && !(modules & PL_MODULE_LAMBERT_CONFORMAL_CONIC))
                continue;
            if (strcmp(name, "pl_project_mercator.opencl") == 0 && !(modules & PL_MODULE_MERCATOR))
                continue;
            if (strcmp(name, "pl_project_robinson.opencl") == 0 && !(modules & PL_MODULE_ROBINSON))
                continue;
            if (strcmp(name, "pl_project_transverse_mercator.opencl") == 0 && !(modules & PL_MODULE_TRANSVERSE_MERCATOR))
                continue;
            if (strcmp(name, "pl_project_winkel_tripel.opencl") == 0 && !(modules & PL_MODULE_WINKEL_TRIPEL))
                continue;
          
            if (strlen(path) + strlen(name) + 2 < sizeof(filename)) {
				pointers[entry_index++] = buffer + buf_used;
				sprintf(filename, "%s/%s", path, name);
				int fd = open(filename, O_RDONLY);
				if (fd == -1) {
					continue;
				}
				while ((bytes_read = read(fd, buffer + buf_used, sizeof(buffer) - buf_used - 1)) > 0) {
					buf_used += bytes_read;
				}
				buffer[buf_used++] = '\0';
				close(fd);
                
                char *p = pointers[entry_index-1];
                //printf("THINGY WE B SEARCHIN FOR KERNELS IN:\n%s\n", p);
				while ((p = strstr(p, "__kernel")) != NULL) {
					kernel_count++;
          //printf("FOUND A KERNY\n");
                    p += sizeof("__kernel")-1;
				}
			}
		}
	} 
	
	closedir(dir);
	
	if (entry_index == 0) {
  //printf("entry index fuckup??\n");
        if (outError)
            *outError = 2;
		return NULL;
	}
	
	pointers[entry_index] = NULL;
  //printf("create prog with source....?\n");
	program = clCreateProgramWithSource(pl_ctx->ctx, entry_index, (const char **)pointers, NULL, &error);
	//printf("create prog with source SUCCESS?\n");

	if (error != CL_SUCCESS) {
    //printf("LOLJK NOP EPIC FAIL\n");
		if (outError != NULL)
			*outError = error;
		return NULL;
	}
	//printf("buildin program....\n");
	error = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
		//printf("buildin program SUCCESS\n");

	if (error != CL_SUCCESS) {
      //printf("LOLJK NOP EPIC FAIL\n");

		size_t error_len;
		char error_buf[4*8192];
		
		//printf("Error: Failed to build program executable!\n");
		
		clGetProgramBuildInfo(program, pl_ctx->device_id, CL_PROGRAM_BUILD_LOG, sizeof(error_buf), 
							  error_buf, &error_len);
		
		//printf("%s\n", error_buf);
		if (outError != NULL)
			*outError = error;
		clReleaseProgram(program);
		return NULL;
	}
	//printf("1\n");
	size_t binary_length;
  //printf("1\n");
	clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &binary_length, NULL);
	//printf("1\n");
	u_char *binary;
  //printf("1\n");
	if ((binary = malloc(binary_length)) == NULL) {
    //printf("512312\n");
		clReleaseProgram(program);
    //printf("LOL SOME SHIT WENT SO WRONG?\n");
		if (outError != NULL)
			*outError = CL_OUT_OF_HOST_MEMORY;
		return NULL;
	}
  //printf("1\n");
	clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(u_char *), &binary, NULL);
	//printf("1\n");
	clReleaseProgram(program);
	//printf("1\n");
	PLCode *pl_code;
	if ((pl_code = malloc(sizeof(PLCode))) == NULL) {
    //printf("LOL SOME SHIT WENT SO WRONG?\n");

		free(binary);
		if (outError != NULL)
			*outError = CL_OUT_OF_HOST_MEMORY;
		return NULL;
	}
  //printf("FINAL KK: %d\n", kernel_count);
	//printf("1\n");
	pl_code->binary = binary;
	pl_code->len = binary_length;
	pl_code->kernel_count = kernel_count;
  //printf("1\n");
    if (outError)
        *outError = CL_SUCCESS;
	if(*outError == CL_SUCCESS) {
    //printf("BUT WAT WE SUCCEEDED WTFBBQ??\n");
  }
	return pl_code;
}

cl_int pl_load_code(PLContext *pl_ctx, PLCode *pl_code) {
	cl_program program;
	cl_int error;
	
	cl_int binary_status;
	//printf("WE BE LOADIN SOME CODE KIDDOS\n");
	program = clCreateProgramWithBinary(pl_ctx->ctx, 1, 
										(const cl_device_id *)&pl_ctx->device_id, 
										(const size_t *)&pl_code->len, 
										(const u_char **)&pl_code->binary, 
										&binary_status, &error);
  //printf("WELL WE DIDNT CRASH?\n");
	if (error != CL_SUCCESS) {
    //printf("AH SOMEONE GONEE FUKED UP\n");
		return error;
	}
	
	error = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	
	if (error != CL_SUCCESS) {
    //printf("NOTHIN BEATS OLD SKEWL DEBUGGIN OH YEAH B T DUBS, SHIT HIT THE FAN TRYIN TO BUILD DAT PROGGY\n");
		clReleaseProgram(program);
		return error;
	}
	
	cl_uint kernel_count_ret;
	
	cl_kernel *kernels;
	if ((kernels = malloc(sizeof(cl_kernel) * pl_code->kernel_count)) == NULL) {
    //printf("out of mem so soon? do u even lift?? \n");
		clReleaseProgram(program);
		return CL_OUT_OF_HOST_MEMORY;
	}
	
	error = clCreateKernelsInProgram(program, pl_code->kernel_count, kernels, &kernel_count_ret);
	
	clReleaseProgram(program);
	
	if (error != CL_SUCCESS) {
    //printf("ERROR POPPIN SUM KERNELS IN DIS PROGGIE PROBS SHOULD GIT DAT CHECKED OUT YO\n");
    if(error == CL_INVALID_PROGRAM) {
      //printf("WELL WE GOT SUM INVALID PROGGIES UP IN HERE\n");
    }
    if(error == CL_INVALID_PROGRAM_EXECUTABLE) {
      //printf("WE GOT SUM INVALID PROGGIE EXES UP IN HERE YO, DIS SHIT BE CRAY\n");
    }
    if(error == CL_INVALID_VALUE) {
      //printf("YO KID, YOUR VALUE IS INVALID GTFO!\n");
      //printf("%d code->kernel_count vs %d kernel_count_ret\n", pl_code->kernel_count, kernel_count_ret);
    }
		free(kernels);
		return error;
	}
	
	pl_ctx->kernel_count = kernel_count_ret;
	pl_ctx->kernels = kernels;
	
	return CL_SUCCESS;
}

void pl_unload_code(PLContext *pl_ctx) {
	int i;
	for (i=0; i<pl_ctx->kernel_count; i++) {
		clReleaseKernel(pl_ctx->kernels[i]);
	}
	free(pl_ctx->kernels);
	
	pl_ctx->kernel_count = 0;
	pl_ctx->kernels = NULL;
}

void pl_release_code(PLCode *pl_code) {
    if (!pl_code)
        return;
    if (pl_code->binary)
        free(pl_code->binary);
    free(pl_code);
}
