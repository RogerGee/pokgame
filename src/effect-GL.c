/* effect-GL.c - pokgame */
#ifdef POKGAME_WIN32
/* have to include Windows.h before OpenGL headers */
#include <Windows.h>
#endif
#include <GL/gl.h>

/* pok_fadeout_effect */
void pok_fadeout_effect_render(struct pok_graphics_subsystem* sys,const struct pok_fadeout_effect* effect)
{
    if (effect->_base.update) {
        if (effect->kind == pok_fadeout_black_screen) {
            /* set color to black; include alpha */
            glColor4f(BLACK_PIXEL_FLOAT[0],BLACK_PIXEL_FLOAT[1],BLACK_PIXEL_FLOAT[2],effect->alpha);
            glBegin(GL_QUADS);
            {
                glVertex2i(0,0);
                glVertex2i(sys->wwidth,0);
                glVertex2i(sys->wwidth,sys->wheight);
                glVertex2i(0,sys->wheight);
            }
            glEnd();
        }
        else if (effect->kind == pok_fadeout_to_center) {
            /* draw 4 quadrilaterals */
            glColor3b(BLACK_PIXEL.r,BLACK_PIXEL.g,BLACK_PIXEL.b);
            glBegin(GL_QUAD_STRIP);
            { /* 10 verteces define 4 quadrilaterals */
                glVertex2f(0.0,0.0);
                glVertex2f(effect->hs[2],effect->hs[0]);
                glVertex2f((GLfloat)sys->wwidth,0.0);
                glVertex2f(effect->hs[3],effect->hs[0]);
                glVertex2f((GLfloat)sys->wwidth,sys->wheight);
                glVertex2f(effect->hs[3],effect->hs[1]);
                glVertex2f(0.0,(GLfloat)sys->wheight);
                glVertex2f(effect->hs[2],effect->hs[1]);
                glVertex2f(0.0,0.0);
                glVertex2f(effect->hs[2],effect->hs[0]);
            }
            glEnd();
        }
    }
    else if (effect->keep) {
        /* keep the screen faded out */
        glColor3b(BLACK_PIXEL.r,BLACK_PIXEL.g,BLACK_PIXEL.b);
        glBegin(GL_QUADS);
        {
            glVertex2i(0,0);
            glVertex2i(sys->wwidth,0);
            glVertex2i(sys->wwidth,sys->wheight);
            glVertex2i(0,sys->wheight);
        }
        glEnd();
    }
}
