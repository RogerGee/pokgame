#include <stdlib.h>
#include <stdint.h>

extern const char* programName;

enum compiled_image_format
{
    COMPILED_RECTANGLE, /* tiles are stored in rows of 16 for viewing */
    COMPILED_SEQUENTIAL /* tiles are stored sequentially for reading into pokgame */
};

void load_image_rgb(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput);
void save_image_rgb(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput);
void load_image_rgba(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput);
void save_image_rgba(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput);

