#include "render_commands.h"

void push_draw_mesh(DrawList* draw_list, MeshHandle handle, Transform transform)
{
    DrawCommand* cmd = &draw_list->commands[draw_list->count++];

    cmd->type = DRAW_MESH;
    cmd->as.draw_mesh.mesh = handle;
    cmd->as.draw_mesh.transform = transform;
}