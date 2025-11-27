# Flare
A real-time hybrid rendering sample featuring GPU culling, deferred shading, PBR, TAA, and ray-traced shadow

## Examples
![Hybrid-Renderer](docs/sponza_screenshot_01.png)

## Features
- GPU-driven rendering
  - Frustum culling
  - Occlusion culling
  - Indirect draw
- Deferred shading
- Physically based rendering
  - Lambert diffuse BRDF
  - Microfacet Cook-Torrance specular BRDF
- Image-based lighting
  - Irradiance map
  - Prefilter map
  - BRDF lut
- Global illumination
  - Dynamic diffuse global illumination
  - Probe visualization
- Reflection
  - Screen space reflection
- Shadow
  - Ray-traced soft shadow
  - SVGF denoise
- Ambient Occlusion
  - Ground-truth ambient occlusion
- Post processing
  - Temporal anti-aliasing
  - ACES filmic tone mapping
- UI
  - real-time tweak panel
  - GPU profiler

## Usage
* `W`/`A`/`S`/`D` - camera movement
* `LMB` - hold to look around
* `F1` - toggle UI
