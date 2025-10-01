#include "texture.h"

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureHandle load_texture(const char* name, Api* api)
{
    char texture_path[512];
    snprintf(texture_path, sizeof(texture_path), "content/Textures/%s", name);

    size_t size;
    uint8_t* buffer = api->load_entire_file(texture_path, &size);

    int x, y, num_channels;
    uint8_t* rgba_data = stbi_load_from_memory(buffer, (int)size, &x, &y, &num_channels, 4);

    return api->create_texture(rgba_data, x, y);
}