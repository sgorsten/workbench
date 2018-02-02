#include "gizmo.h"
#include "pbr.h"

gizmo::gizmo(const rhi::pipeline & pipe, const mesh_asset * arrow_x, const mesh_asset * arrow_y, const mesh_asset * arrow_z) : pipe{&pipe}, arrows{arrow_x, arrow_y, arrow_z}
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
        if(auto hit = arrows[0]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_x; best_t = hit->t; }
        if(auto hit = arrows[1]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_y; best_t = hit->t; }
        if(auto hit = arrows[2]->raycast(ray); hit && hit->t < best_t) { mouseover_mode = gizmo_mode::translate_z; best_t = hit->t; }
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
    cmd.clear_depth(1.0);
    cmd.bind_pipeline(*pipe);

    auto object_set = pool.alloc_descriptor_set(*pipe, pbr::object_set_index);
    object_set.write(0, pbr::object_uniforms{translation_matrix(position)});
    object_set.bind(cmd);

    float3 colors[] {{1,0,0},{0,1,0},{0,0,1}};
    switch(mode != gizmo_mode::none ? mode : mouseover_mode)
    {
    case gizmo_mode::translate_x: colors[0] = {1.0f, 0.5f, 0.5f}; break;
    case gizmo_mode::translate_y: colors[1] = {0.5f, 1.0f, 0.5f}; break;
    case gizmo_mode::translate_z: colors[2] = {0.5f, 0.5f, 1.0f}; break;
    }
    for(int i=0; i<3; ++i)
    {
        auto material_set = pool.alloc_descriptor_set(*pipe, pbr::material_set_index);
        material_set.write(0, pbr::material_uniforms{colors[i],0.8f,0.0f});        
        material_set.bind(cmd); 
        arrows[i]->gmesh.draw(cmd);
    }
}
