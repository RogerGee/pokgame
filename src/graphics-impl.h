/* graphics-impl - pokgame */
#ifndef POKGAME_GRAPHICS_IMPL
#define POKGAME_GRAPHICS_IMPL
#include "graphics.h"

/* store image objects that are potential OpenGL textures */
struct texture_info
{
    int count;
    struct pok_image** images;
};

/* store OpenGL texture references; this is just a list of integers */
struct gl_texture_info
{
    size_t textureAlloc, textureCount;
    uint32_t* textureNames;
};

/* these functions implement platform-specific graphics subsystem operations */
bool_t impl_new(struct pok_graphics_subsystem* sys);
void impl_free(struct pok_graphics_subsystem* sys);
void impl_reload(struct pok_graphics_subsystem* sys);
void impl_load_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count);
void impl_delete_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count);
void impl_set_game_state(struct pok_graphics_subsystem* sys,bool_t state);
void impl_map_window(struct pok_graphics_subsystem* sys);
void impl_unmap_window(struct pok_graphics_subsystem* sys);
void impl_lock(struct pok_graphics_subsystem* sys);
void impl_unlock(struct pok_graphics_subsystem* sys);

/* OpenGL operations */
void gl_init(int32_t viewWidth,int32_t viewHeight);
void gl_create_textures(struct gl_texture_info* info,struct texture_info* texinfo,int count);
void gl_delete_textures(struct gl_texture_info* existing,struct texture_info* info,int count);

#endif
