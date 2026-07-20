/*
 * Combined STB + JXL implementation file.
 * Defines the IMPLEMENTATION macros and includes all header-only libraries.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_DDS_IMPLEMENTATION
#include "stb_image_dds.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image_write_hdr_png.h"

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h"
