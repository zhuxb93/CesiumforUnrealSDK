# CesiumforUnrealSDK

[中文](README.md)

CesiumforUnrealSDK is an independent Unreal Engine 5.7 C++ plugin archive built around CesiumForUnreal. It collects reusable work for a globe camera controller, Unreal-side vector-tile rendering, GeoJSON geometry construction, UMG search/HUD/POI labels, and a generic token-refresh skeleton.

The code has been reorganized into the `GeoEarth` plugin module and is intended to be placed under a UE project's `Plugins/` directory. It does not include the CesiumForUnreal plugin itself, commercial art assets, sqlite caches, build artifacts, real service URLs, or proprietary authentication algorithms.

## Highlights

- `Source/GeoEarth/Camera/`: an `APawn` globe camera with free-flight/follow modes, pan, zoom, rotation, tilt, double-click flight, and Blueprint camera-motion APIs.
- `Source/GeoEarth/VectorTile/`: vector-tile download, LZ4 decompression, FlatBuffers parsing, async building/road mesh generation, ProceduralMesh output, and frame-rate-aware throttling.
- `Source/GeoEarth/GeoJSON/`: GeoJSON parsing, geometry construction, and multi-source loading.
- `Source/GeoEarth/UI/`: city search panels, longitude/latitude/distance HUD, world-space POI labels, and search-panel structure.
- `Source/GeoEarth/Auth/`: a generic scheduled refresh and request-header/URL placeholder injection skeleton without real signing logic.
- `Docs/`: Chinese engineering notes for the camera controller, vector tiles, and UMG structure.

## Installation And Dependencies

1. Put this repository under `Plugins/CesiumforUnrealSDK/` in an Unreal Engine project.
2. Install and enable CesiumForUnreal, and make sure the project can reference `CesiumRuntime`.
3. Enable or include `ProceduralMeshComponent`, `UMG`, `HTTP`, `Json`, and related module dependencies.
4. Regenerate project files and compile the `GeoEarth` module.
5. Example URLs use `https://example.com/...` placeholders. Replace them with your own public test service or local service.

## Usage Notes

Start with the camera controller in a clean UE project: create a level with `CesiumGeoreference`, set `AGeoCameraController` as the default Pawn, and verify flight controls first. After that, use synthetic GeoJSON or local test tiles to validate the `VectorTile` and `UI` modules.

The `Auth` module is only a safe public skeleton. For real authentication, keep secrets and signing logic on the server side and let the client store only short-lived access credentials.

## Sanitization And Licensing

- The module has been renamed to `GeoEarth`, and class prefixes have been normalized to `Geo`.
- Private brand names, internal URLs, real city-service endpoints, proprietary signing logic, caches, and commercial assets have been removed.
- `LICENSE` only covers original or rewritten code in this repository.
- CesiumForUnreal, Google FlatBuffers, Triangle(C), and Unreal Engine related parts remain governed by their own licenses. See `THIRD_PARTY_NOTICES.md`.
- Triangle(C) has a non-commercial licensing boundary and is listed in `待本人确认清单.md` as an item to review before public promotion.
- See `脱敏复核报告.md` for the sanitization review.

## Related Repositories

- `VectorTile` can be compared with the C# vector-tile pipeline in `CesiumforUnitySDK`.
- `CoordinateConverter` can be compared with `UnityGeoToolkit`'s `GeoMath`.
- The globe camera controller can be read alongside the Unity camera keyframe player.

## Current Status

The plugin restructuring, Chinese module notes, English entry document, third-party notices, and sanitization review are complete. The plugin has not yet been imported and compiled in Unreal Editor; run a UE 5.7 plugin build before production use.
