in float v_placed;
in float v_radius;
in highp vec2 v_extrude;

void main() {
    float alpha = 0.5;
    float stroke_radius = 0.9;

    float distance_to_center = length(v_extrude);
    float distance_to_edge = abs(distance_to_center - v_radius);
    float opacity_t = smoothstep(-stroke_radius, 0.0, -distance_to_edge);

    // Red = collision, hide label. Blue = no collision, label is showing.
    vec4 color = v_placed > 0.5 ? vec4(0.0, 0.0, 1.0, 0.5) : vec4(1.0, 0.0, 0.0, 1.0);

    fragColor = color * alpha * opacity_t;
}
