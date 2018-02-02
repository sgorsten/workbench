// Implementation of a 3D editor gizmo
// Currently tied strongly to our asset pipeline and our PBR rendering system. In the future we 
// will separate out these concerns to allow the gizmo to be used with other rendering styles.
#pragma once
#include "asset.h"
#include "camera.h"
#include "gui.h"

enum class gizmo_mode { none, translate_x, translate_y, translate_z, translate_yz, translate_zx, translate_xy };
struct gizmo
{
    // Rendering state
    rhi::ptr<const rhi::pipeline> pipe;
    const mesh_asset * arrows[3];
    gizmo_mode mode, mouseover_mode;
    float3 click_offset, original_position;

    void plane_translation_dragger(gui & g, const rect<int> & viewport, const camera & cam, const float3 & plane_normal, float3 & point) const;
    void axis_translation_dragger(gui & g, const rect<int> & viewport, const camera & cam, const float3 & axis, float3 & point) const;
public:
    gizmo(const rhi::pipeline & pipe, const mesh_asset * arrow_x, const mesh_asset * arrow_y, const mesh_asset * arrow_z);

    void draw(rhi::command_buffer & cmd, gfx::transient_resource_pool & pool, const float3 & position) const;
    void position_gizmo(gui & g, int id, const rect<int> & viewport, const camera & cam, float3 & position);
};