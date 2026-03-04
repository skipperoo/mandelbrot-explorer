#ifndef _COMMON_H
#define _COMMON_H
#include <string.h>
#include <errno.h>

#define CHECKALLOC(pointer) if(pointer == NULL) {fprintf(stderr, "Out Of Memory (file %s, line %d): "#pointer"\n", __FILE__, __LINE__);exit(EXIT_FAILURE);}
#define CHECKSC(call) \
	if(call < 0){ \
    fprintf(stderr, "Error (file %s, line %d): %s\n", __FILE__, __LINE__, strerror(errno)); \
		exit(EXIT_FAILURE); \
	}
#define CHECKPTHREAD(res) \
	if(res != 0){ \
    fprintf(stderr, "Error (file %s, line %d): %s\n", __FILE__, __LINE__, strerror(res)); \
		exit(EXIT_FAILURE); \
	}
typedef enum { SCALAR, AVX, INTEL_GPU, NVIDIA_GPU } RenderMode;



#endif
