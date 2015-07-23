/* primatives.h - pokgame */
#ifndef POKGAME_PRIMATIVES_H
#define POKGAME_PRIMATIVES_H
#include "opengl.h"

void pok_primative_setup_modelview(int32_t x,int32_t y,int32_t width,int32_t height);

/* Shadow Ellipse: a GL_POLYGON used to render a shadow or hole */
#define POK_SHADOW_ELLIPSE_VERTEX_COUNT 360
extern const GLfloat POK_SHADOW_ELLIPSE[];

/* Box: a GL_QUADS used to render a filled box shape or a GL_LINE_LOOP used to render a wire box */
#define POK_BOX_VERTEX_COUNT 4
extern const GLfloat POK_BOX[];

#endif
