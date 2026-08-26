layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_extrude;
layout (location = 2) in vec2 a_placed;
layout (location = 3) in vec3 a_measured_box;

layout (std140) uniform CollisionDrawableUBO {
    highp mat4 u_matrix;
};

layout (std140) uniform CollisionTilePropsUBO {
    highp vec2 u_pixel_extrude_scale;
    highp float drawable_pad1;
    highp float drawable_pad2;
};

out float v_placed;
out float v_radius;
out highp vec2 v_extrude;

void main() {
    // The sign of the extrusion picks the quad corner; the radius the collision index measured
    // for this circle sizes it, in viewport pixels.
    vec2 quadVertexOffset = vec2(a_extrude.x < 0.0 ? -1.0 : 1.0, a_extrude.y < 0.0 ? -1.0 : 1.0);

    highp float padding_factor = 1.2; // Pad the vertices slightly to make room for anti-alias blur
    highp vec2 quadVertexExtent = quadVertexOffset * a_measured_box.z * padding_factor;

    gl_Position = u_matrix * vec4(a_pos, 0.0, 1.0);
    gl_Position.xy += quadVertexExtent * u_pixel_extrude_scale * 2.0 * gl_Position.w;

    v_placed = a_placed.x;
    v_radius = a_measured_box.z;
    v_extrude = quadVertexExtent;
}
