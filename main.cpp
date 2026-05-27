#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>

constexpr unsigned int WIDTH = 1500;
constexpr unsigned int HEIGHT = 950;

const std::string fragmentShader = R"(

uniform vec2 u_resolution;
uniform float u_time;
uniform float u_mass;
uniform float u_camHeight;
uniform float u_camDist;

#define PI 3.14159265359

float sat(float x) {
    return clamp(x, 0.0, 1.0);
}

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 34.45);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;

    for (int i = 0; i < 5; i++) {
        v += a * valueNoise(p);
        p *= 2.0;
        a *= 0.5;
    }

    return v;
}

float softStarLayer(vec2 uv, float scale, float threshold, float sizeMin, float sizeMax) {
    vec2 p = uv * scale;
    vec2 g = floor(p);
    vec2 f = fract(p);

    float h = hash(g);
    if (h < threshold) return 0.0;

    vec2 center = vec2(hash(g + 1.37), hash(g + 7.91));
    float radius = mix(sizeMin, sizeMax, hash(g + 12.4));
    float d = length(f - center);

    float star = smoothstep(radius, 0.0, d);
    star *= 0.5 + 0.5 * hash(g + 19.7);

    return star;
}

vec3 backgroundFromDirection(vec3 dir) {
    dir = normalize(dir);

    float lon = atan(dir.z, dir.x);
    float lat = asin(clamp(dir.y, -1.0, 1.0));

    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, lat / PI + 0.5);

    vec3 col = vec3(0.003, 0.004, 0.010);

    // Big smooth galactic light field.
    // This is the main fix: the black hole now bends continuous light,
    // not just isolated star pixels.
    float bandCenter = lat + 0.14 * sin(lon * 1.6 + 0.4);
    float galaxyBand = exp(-pow(bandCenter / 0.24, 2.0));

    float clouds1 = fbm(vec2(lon * 2.4, lat * 6.8) + vec2(0.025 * u_time, -0.015 * u_time));
    float clouds2 = fbm(vec2(lon * 5.0 + 4.1, lat * 10.0 - 2.2));

    col += vec3(0.026, 0.030, 0.055) * galaxyBand * (0.45 + 0.55 * clouds1);
    col += vec3(0.014, 0.018, 0.035) * galaxyBand * 0.45 * clouds2;

    // Soft nebula, low-frequency only.
    float nebula =
        0.5 + 0.5 * sin(uv.x * 7.0 + u_time * 0.015) *
        sin(uv.y * 9.0 - u_time * 0.012);

    col += vec3(0.008, 0.011, 0.022) * nebula * 0.25;

    // Fewer, softer stars so the lensing is less grainy.
    float s1 = softStarLayer(uv, 145.0, 0.9962, 0.09, 0.22);
    float s2 = softStarLayer(uv + vec2(0.13, 0.07), 250.0, 0.9978, 0.05, 0.13);
    float s3 = softStarLayer(uv + vec2(0.29, 0.19), 390.0, 0.9990, 0.025, 0.065);

    float stars = s1 * 0.75 + s2 * 0.55 + s3 * 0.38;
    col += vec3(1.0) * stars;

    return col;
}

float potential(vec3 p, vec3 bh, float rs) {
    float r = length(p - bh);
    return -0.5 * rs / max(r, rs * 0.72);
}

vec3 potentialGradient(vec3 p, vec3 bh, float rs) {
    float e = 0.014;

    float px1 = potential(p + vec3(e, 0.0, 0.0), bh, rs);
    float px0 = potential(p - vec3(e, 0.0, 0.0), bh, rs);

    float py1 = potential(p + vec3(0.0, e, 0.0), bh, rs);
    float py0 = potential(p - vec3(0.0, e, 0.0), bh, rs);

    float pz1 = potential(p + vec3(0.0, 0.0, e), bh, rs);
    float pz0 = potential(p - vec3(0.0, 0.0, e), bh, rs);

    return vec3(
        (px1 - px0) / (2.0 * e),
        (py1 - py0) / (2.0 * e),
        (pz1 - pz0) / (2.0 * e)
    );
}

float singleWell(vec2 xz, vec2 bhXZ, float rs) {
    float r = length(xz - bhXZ);
    return 1.10 * rs / max(r, 0.33);
}

float gridHeight(vec2 xz, vec3 bh, float rs) {
    float well = singleWell(xz, bh.xz, rs);
    well = clamp(well, 0.0, 2.25);

    float ripple = 0.030 * sin(6.0 * length(xz - bh.xz) - 1.5 * u_time) *
                   exp(-0.18 * length(xz - bh.xz));

    return -1.45 - well + ripple;
}

vec3 gridNormal(vec2 xz, vec3 bh, float rs) {
    float e = 0.010;

    float h  = gridHeight(xz, bh, rs);
    float hx = gridHeight(xz + vec2(e, 0.0), bh, rs);
    float hz = gridHeight(xz + vec2(0.0, e), bh, rs);

    vec3 dx = vec3(e, hx - h, 0.0);
    vec3 dz = vec3(0.0, hz - h, e);

    return normalize(cross(dz, dx));
}

vec3 gridColor(vec3 p, vec3 n, vec3 ro, vec3 bh, float rs) {
    vec2 uv = p.xz;

    float gx = abs(fract(uv.x * 0.55) - 0.5);
    float gz = abs(fract(uv.y * 0.55) - 0.5);

    float lineX = smoothstep(0.050, 0.012, gx);
    float lineZ = smoothstep(0.050, 0.012, gz);
    float grid = max(lineX, lineZ);

    float distToBH = length(uv - bh.xz);
    float glow = 1.0 / (1.0 + 0.75 * distToBH * distToBH);

    vec3 lightDir = normalize(vec3(-0.35, 1.0, -0.22));
    float diff = 0.22 + 0.78 * max(dot(n, lightDir), 0.0);

    vec3 base = vec3(0.004, 0.008, 0.018);
    vec3 lines = mix(vec3(0.35, 0.55, 0.90), vec3(0.95, 0.98, 1.0), glow);

    vec3 col = base * diff;
    col += lines * grid * diff * (0.65 + 1.15 * glow);

    float fade = exp(-0.040 * length(p - ro));
    col *= fade;

    return col;
}

vec3 diskNormalVec() {
    return normalize(vec3(0.0, 1.0, 0.0));
}

vec3 diskEmission(vec3 pos, vec3 bh, vec3 rayDir, float rs) {
    vec3 n = diskNormalVec();
    vec3 rel = pos - bh;

    float h = dot(rel, n);
    vec3 planar = rel - n * h;
    float radial = length(planar);

    float inner = rs * 1.65;
    float outer = rs * 5.45;

    if (abs(h) > 0.012 || radial < inner || radial > outer) {
        return vec3(0.0);
    }

    float phi = atan(planar.z, planar.x);

    float radialMask =
        smoothstep(inner, inner + 0.06, radial) *
        smoothstep(outer, outer - 0.18, radial);

    // Less noisy disk. Mostly continuous light, only mild banding.
    float bands =
        0.82
        + 0.12 * sin(phi * 11.0 + u_time * 1.2)
        + 0.06 * sin(radial * 9.0 - u_time * 2.0);

    vec3 tangent = normalize(cross(n, normalize(planar)));
    float doppler = 0.5 + 0.5 * dot(tangent, -rayDir);

    vec3 hot = mix(vec3(1.0, 0.93, 0.78), vec3(1.0), doppler);

    float redshift = sqrt(max(1.0 - rs / max(radial, rs * 1.04), 0.16));

    float intensity =
        radialMask *
        (0.42 + 1.70 * doppler) *
        bands *
        redshift;

    return hot * intensity;
}

void traceScene(
    vec3 ro,
    vec3 rd,
    vec3 bh,
    float rs,
    out bool captured,
    out bool hitGrid,
    out vec3 gridPos,
    out vec3 gridN,
    out vec3 escapedDir,
    out vec3 diskLight,
    out float whiteHalo,
    out float minRadius
) {
    captured = false;
    hitGrid = false;
    gridPos = vec3(0.0);
    gridN = vec3(0.0);
    escapedDir = rd;
    diskLight = vec3(0.0);
    whiteHalo = 0.0;
    minRadius = 1e9;

    vec3 pos = ro;
    vec3 dir = normalize(rd);

    float bCrit = 1.5 * sqrt(3.0) * rs;
    float prevSigned = pos.y - gridHeight(pos.xz, bh, rs);

    for (int i = 0; i < 620; i++) {
        float r = length(pos - bh);
        minRadius = min(minRadius, r);

        if (r < rs) {
            captured = true;
            escapedDir = dir;
            return;
        }

        vec3 emission = diskEmission(pos, bh, dir, rs);
        diskLight += emission * 0.058;

        vec3 gradPhi = potentialGradient(pos, bh, rs);
        vec3 transverseGrad = gradPhi - dir * dot(gradPhi, dir);

        // Better near the hole, but still cheap.
        float ds = mix(0.012, 0.052, sat(r / (8.0 * rs)));
        dir = normalize(dir - 2.95 * transverseGrad * ds);

        float caustic = exp(-24.0 * abs(r - bCrit));
        whiteHalo += caustic * 0.0034;

        vec3 nextPos = pos + dir * ds;
        float newSigned = nextPos.y - gridHeight(nextPos.xz, bh, rs);

        if (prevSigned > 0.0 && newSigned <= 0.0) {
            hitGrid = true;
            gridPos = nextPos;
            gridN = gridNormal(nextPos.xz, bh, rs);
            escapedDir = dir;
            return;
        }

        pos = nextPos;
        prevSigned = newSigned;

        if (length(pos - ro) > 70.0 || abs(pos.x) > 48.0 || abs(pos.y) > 36.0 || pos.z > 62.0) {
            break;
        }
    }

    escapedDir = dir;
}

vec3 cheapBloom(vec3 color, vec3 diskLight, float whiteHalo, float minRadius, float rs) {
    float diskBloom = clamp(length(diskLight) * 0.24, 0.0, 1.2);
    float haloBloom = clamp(whiteHalo * 0.55, 0.0, 1.0);

    // Extra white glow around the lensing zone, not global supersampling.
    float lensZone = exp(-1.2 * abs(minRadius - 2.6 * rs));
    float lensBloom = lensZone * 0.045;

    color += vec3(1.0) * diskBloom * 0.12;
    color += vec3(1.0) * haloBloom * 0.18;
    color += vec3(1.0) * lensBloom;

    return color;
}

void main() {
    vec2 frag = gl_FragCoord.xy;
    vec2 uv = (2.0 * frag - u_resolution.xy) / u_resolution.y;

    vec3 ro = vec3(0.0, u_camHeight, -u_camDist);
    vec3 target = vec3(0.0, -0.18, 0.0);

    vec3 forward = normalize(target - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));

    float focalLength = 1.74;
    vec3 rd = normalize(forward * focalLength + right * uv.x + up * uv.y);

    vec3 bh = vec3(0.0, 0.0, 0.0);
    float rs = 0.38 * u_mass;

    bool captured;
    bool hitGrid;
    vec3 gridPos;
    vec3 gridN;
    vec3 escapedDir;
    vec3 diskLight;
    float whiteHalo;
    float minRadius;

    traceScene(
        ro, rd, bh, rs,
        captured, hitGrid, gridPos, gridN,
        escapedDir, diskLight, whiteHalo, minRadius
    );

    vec3 color;

    if (captured) {
        color = vec3(0.0);
    } else if (hitGrid) {
        color = gridColor(gridPos, gridN, ro, bh, rs);
        color += backgroundFromDirection(escapedDir) * 0.10;
    } else {
        color = backgroundFromDirection(escapedDir);

        float shadow = sat((3.2 * rs - minRadius) / (2.0 * rs));
        color *= 1.0 - 0.42 * shadow;
    }

    color += diskLight;

    // White halo only.
    color += vec3(1.0) * clamp(whiteHalo, 0.0, 0.85);

    // Cheap bloom instead of 5x supersampling.
    color = cheapBloom(color, diskLight, whiteHalo, minRadius, rs);

    float vignette = 1.0 - 0.10 * dot(uv * 0.72, uv * 0.72);
    color *= vignette;

    color = pow(max(color, vec3(0.0)), vec3(0.92));

    gl_FragColor = vec4(color, 1.0);
}

)";

int main() {
    sf::RenderWindow window(
        sf::VideoMode({WIDTH, HEIGHT}),
        "Single Black Hole Halo + Smooth Lensing"
    );

    window.setFramerateLimit(144);

    sf::Shader shader;
    if (!shader.loadFromMemory(fragmentShader, sf::Shader::Type::Fragment)) {
        std::cerr << "Failed to load fragment shader.\n";
        return 1;
    }

    sf::RectangleShape screen({static_cast<float>(WIDTH), static_cast<float>(HEIGHT)});

    float mass = 1.0f;
    float cameraHeight = 0.92f;
    float cameraDistance = 8.7f;

    sf::Clock clock;

    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            mass += 0.01f;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            mass -= 0.01f;
            if (mass < 0.45f) {
                mass = 0.45f;
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            cameraHeight += 0.01f;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            cameraHeight -= 0.01f;
            if (cameraHeight < 0.18f) {
                cameraHeight = 0.18f;
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            cameraDistance += 0.03f;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            cameraDistance -= 0.03f;
            if (cameraDistance < 4.0f) {
                cameraDistance = 4.0f;
            }
        }

        shader.setUniform("u_resolution", sf::Glsl::Vec2(static_cast<float>(WIDTH), static_cast<float>(HEIGHT)));
        shader.setUniform("u_time", clock.getElapsedTime().asSeconds());
        shader.setUniform("u_mass", mass);
        shader.setUniform("u_camHeight", cameraHeight);
        shader.setUniform("u_camDist", cameraDistance);

        sf::RenderStates states;
        states.shader = &shader;

        window.clear();
        window.draw(screen, states);
        window.display();
    }

    return 0;
}