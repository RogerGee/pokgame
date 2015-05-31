#include "lib.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>

void load_image_rgb(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput)
{
    size_t pixcnt;
    FILE* fin;
    fin = fopen(file,"r");
    if (fin == NULL) {
        fprintf(stderr,"%s: could not open image file: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    pixcnt = width * height;
    if (showOutput)
        printf("loading image [%s]: %zux%zu pixels [%zu total]\n",file,width,height,pixcnt);
    if (fread(pix,3,pixcnt,fin) != pixcnt) {
        fprintf(stderr,"%s: failed to read image\n",programName);
        exit(EXIT_FAILURE);
    }
    fclose(fin);
}

void save_image_rgb(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput)
{
    FILE* fout;
    size_t pixcnt;
    fout = fopen(file,"w");
    if (fout == NULL) {
        fprintf(stderr,"%s: could not open output file %s\n",programName,file);
        exit(EXIT_FAILURE);
    }
    pixcnt = width*height;
    if (showOutput)
        printf("saving image [%s]: %zux%zu pixels [%zu total]\n",file,width,height,pixcnt);
    if (fwrite(pix,3,pixcnt,fout) != pixcnt) {
        fprintf(stderr,"%s: incomplete or failed write\n",programName);
        exit(EXIT_FAILURE);
    }
    fclose(fout);
}
void load_image_rgba(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput)
{
    size_t pixcnt;
    FILE* fin;
    fin = fopen(file,"r");
    if (fin == NULL) {
        fprintf(stderr,"%s: could not open image file: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    pixcnt = width * height;
    if (showOutput)
        printf("loading image [%s]: %zux%zu pixels [%zu total]\n",file,width,height,pixcnt);
    if (fread(pix,4,pixcnt,fin) != pixcnt) {
        fprintf(stderr,"%s: failed to read image\n",programName);
        exit(EXIT_FAILURE);
    }
    fclose(fin);
}

void save_image_rgba(const char* file,uint8_t* pix,size_t width,size_t height,int showOutput)
{
    FILE* fout;
    size_t pixcnt;
    fout = fopen(file,"w");
    if (fout == NULL) {
        fprintf(stderr,"%s: could not open output file %s\n",programName,file);
        exit(EXIT_FAILURE);
    }
    pixcnt = width*height;
    if (showOutput)
        printf("saving image [%s]: %zux%zu pixels [%zu total]\n",file,width,height,pixcnt);
    if (fwrite(pix,4,pixcnt,fout) != pixcnt) {
        fprintf(stderr,"%s: incomplete or failed write\n",programName);
        exit(EXIT_FAILURE);
    }
    fclose(fout);
}
