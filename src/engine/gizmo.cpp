#include "gizmo.h"
#include "pbr.h"

gizmo::gizmo(const std::array<rhi::ptr<const rhi::pipeline>,5> & passes, const mesh_asset * arrow_x, const mesh_asset * arrow_y, const mesh_asset * arrow_z,
        const mesh_asset * box_yz, const mesh_asset * box_zx, const mesh_asset * box_xy) :
    passes{passes}, meshes{arrow_x, arrow_y, arrow_z, box_yz, box_zx, box_xy}
{
    
}

void gizmo::position_gizmo(gui & g, int id, const rect<int> & viewport, const camera & cam, float3 & position)
{
    // Determine which gizmo the user's mouse is over
    float best_t = std::numeric_limits<float>::infinity();
    auto ray = cam.get_ray_from_pixel(g.get_cursor(), viewport);
    ray.origin -= position;
    mouseover_mode = gizmo_mode::none;
    if(g.is_cursor_over(viewport))
    {
        if(auto hit = meshes[0]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_x; best_t = hit->t; }
        if(auto hit = meshes[1]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_y; best_t = hit->t; }
        if(auto hit = meshes[2]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_z; best_t = hit->t; }
        if(auto hit = meshes[3]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_yz; best_t = hit->t; }
        if(auto hit = meshes[4]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_zx; best_t = hit->t; }
        if(auto hit = meshes[5]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_xy; best_t = hit->t; }
    }

    // On click, set the gizmo mode based on which component the user clicked on
    if(g.is_mouse_clicked() && mouseover_mode != gizmo_mode::none)
    {
        mode = mouseover_mode;
        click_offset = ray.origin + ray.direction*best_t;
        original_position = position + click_offset;
        g.set_pressed(id);
        g.consume_click();
    }

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.is_pressed(id))
    {
        position += click_offset;
        switch(mode)
        {
        case gizmo_mode::translate_x: axis_translation_dragger(g, viewport, cam, {1,0,0}, position); break;
        case gizmo_mode::translate_y: axis_translation_dragger(g, viewport, cam, {0,1,0}, position); break;
        case gizmo_mode::translate_z: axis_translation_dragger(g, viewport, cam, {0,0,1}, position); break;
        case gizmo_mode::translate_yz: plane_translation_dragger(g, viewport, cam, {1,0,0}, position); break;
        case gizmo_mode::translate_zx: plane_translation_dragger(g, viewport, cam, {0,1,0}, position); break;
        case gizmo_mode::translate_xy: plane_translation_dragger(g, viewport, cam, {0,0,1}, position); break;
        }        
        position -= click_offset;
    }

    // On release, deactivate the current gizmo mode
    if(g.check_release(id)) mode = gizmo_mode::none;
}

void gizmo::axis_translation_dragger(gui & g, const rect<int> & viewport, const camera & cam, const float3 & axis, float3 & point) const
{
    // First apply a plane translation dragger with a plane that contains the desired axis and is oriented to face the camera
    const float3 plane_tangent = cross(axis, point - cam.position);
    const float3 plane_normal = cross(axis, plane_tangent);
    plane_translation_dragger(g, viewport, cam, plane_normal, point);

    // Constrain object motion to be along the desired axis
    point = original_position + axis * dot(point - original_position, axis);
}

void gizmo::plane_translation_dragger(gui & g, const rect<int> & viewport, const camera & cam, const float3 & plane_normal, float3 & point) const
{
    // Define the plane to contain the original position of the object
    const float3 plane_point = original_position;

    // Define a ray emitting from the camera underneath the cursor
    const ray ray = cam.get_ray_from_pixel(g.get_cursor(), viewport);

    // If an intersection exists between the ray and the plane, place the object at that point
    const float denom = dot(ray.direction, plane_normal);
    if(std::abs(denom) == 0) return;
    const float t = dot(plane_point - ray.origin, plane_normal) / denom;
    if(t < 0) return;
    point = ray.origin + ray.direction * t;
}

void gizmo::draw(rhi::command_buffer & cmd, gfx::transient_resource_pool & pool, const float3 & position) const
{
    // Determine gizmo colors based on mouseover status
    float3 colors[] {{1,0,0},{0,1,0},{0,0,1},{0,1,1},{1,0,1},{1,1,0}};
    switch(mode != gizmo_mode::none ? mode : mouseover_mode)
    {
    case gizmo_mode::translate_x: colors[0] = {1.0f, 0.5f, 0.5f}; break;
    case gizmo_mode::translate_y: colors[1] = {0.5f, 1.0f, 0.5f}; break;
    case gizmo_mode::translate_z: colors[2] = {0.5f, 0.5f, 1.0f}; break;
    case gizmo_mode::translate_yz: colors[3] = {0.5f, 1.0f, 1.0f}; break;
    case gizmo_mode::translate_zx: colors[4] = {1.0f, 0.5f, 1.0f}; break;
    case gizmo_mode::translate_xy: colors[5] = {1.0f, 1.0f, 0.5f}; break;
    }

    auto object_set = pool.alloc_descriptor_set(*passes[0], pbr::object_set_index);
    object_set.write(0, pbr::object_uniforms{translation_matrix(position)});
    gfx::descriptor_set material_sets[6] {
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index),
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index),
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index),
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index),
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index),
        pool.alloc_descriptor_set(*passes[0], pbr::material_set_index)
    };
    for(int i=0; i<6; ++i) material_sets[i].write(0, pbr::material_uniforms{colors[i]*0.8f,0.8f,0.0f,0.35f});   

    int refs[] {1,0,1,1,1};
    //cmd.clear_depth(1.0);
    object_set.bind(cmd);
    for(size_t j=0; j<5; ++j)
    {
        cmd.set_stencil_ref(refs[j]);
        cmd.bind_pipeline(*passes[j]);
        for(int i=0; i<6; ++i)
        {
            material_sets[i].bind(cmd); 
            meshes[i]->gmesh.draw(cmd);
        }
    }
}
