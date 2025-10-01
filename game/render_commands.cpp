#include "render_commands.h"

void push_draw_mesh(DrawList* draw_list, MeshHandle mesh, TextureHandle texture, Transform transform)
{
    DrawCommand* cmd = &draw_list->commands[draw_list->count++];

    cmd->type = DrawCommandType::DrawMesh;
    cmd->as.draw_mesh.mesh = mesh;
    cmd->as.draw_mesh.texture = texture;
    cmd->as.draw_mesh.transform = transform;
}
