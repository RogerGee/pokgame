/* opengl platform header - pokgame */
#ifndef POKGAME_OPENGL_H
#define POKGAME_OPENGL_H

#ifdef POKGAME_WIN32

/* The Windows implementation of OpenGL does not have this functionality
   for texture mapping; so I work around it using another parameter which
   does the same thing in another way. */
#define GL_TEXTURE_BASE_LEVEL GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAX_LEVEL  GL_TEXTURE_MIN_FILTER
#define TEXPARAM GL_NEAREST

#else

#define TEXPARAM 0

#endif

#if defined(POKGAME_LINUX)
#include <GL/gl.h>

#elif defined(POKGAME_WIN32)
/* have to include Windows.h before OpenGL headers */
#include <Windows.h>
#include <gl/GL.h>

#elif defined(POKGAME_OSX)
#include <OpenGL/gl.h>

#endif

#endif
