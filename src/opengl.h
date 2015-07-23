/* opengl platform header - pokgame */
#ifndef POKGAME_OPENGL_H
#define POKGAME_OPENGL_H

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
