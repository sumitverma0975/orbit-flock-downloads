# Vehicle asset attribution

These three third-party 3D artworks are distributed under Creative Commons Attribution 4.0 International. Retain the artist, source and license credits in redistributed source projects and in a visible credits screen or accompanying credits file in a released game. The artwork license does not imply manufacturer sponsorship or grant trademark rights.

Source mirror: [pedrosoandre/carros](https://github.com/pedrosoandre/carros/tree/2d18dfd9db96bbf0d9cffa949deaf7f571388cbf), immutable commit `2d18dfd9db96bbf0d9cffa949deaf7f571388cbf`. Original author metadata and licenses were independently checked using the public Sketchfab API; snapshots are in `Provenance/`.

License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) — legal text is also included as `LICENSE-CC-BY-4.0.txt`.

## BMW 320i E90

- Original artwork: [BMW 320i E90 Low Polly](https://sketchfab.com/3d-models/bmw-320i-e90-low-polly-28f0bc2f0083417a8e521150529034d6).
- Author: [rifdanzz](https://sketchfab.com/rifdanzz).
- License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- Import file: `bmw-320i.glb`; SHA-256: `a1764d5ab2148790c50f9ca13a76ce8ed08a91295238772f2012603dad9da026`.
- [Exact mirrored download](https://raw.githubusercontent.com/pedrosoandre/carros/2d18dfd9db96bbf0d9cffa949deaf7f571388cbf/assets/bmw-320i.glb); original downloaded file: `bmw-320i.glb`; SHA-256: `a1764d5ab2148790c50f9ca13a76ce8ed08a91295238772f2012603dad9da026`.
- Local changes: Downloaded mirror file unchanged.
- Imported geometry count: 13441 triangles across 49 mesh definitions; 11 materials.

## Mercedes-Benz G-Class W263

- Original artwork: [Mercedes Benz G-class W263](https://sketchfab.com/3d-models/mercedes-benz-g-class-w263-1a2a52b16cad4e618af347461817895c).
- Author: [Lexyc16](https://sketchfab.com/Lexyc16).
- License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- Import file: `mercedes-gclass.glb`; SHA-256: `5d7650132ccb47436d2694a5b84a375ef414cd89ab3e26ac2a6f9f487e0b27a4`.
- [Exact mirrored download](https://raw.githubusercontent.com/pedrosoandre/carros/2d18dfd9db96bbf0d9cffa949deaf7f571388cbf/assets/mercedes-gclass.glb); original downloaded file: `mercedes-gclass.glb`; SHA-256: `5d7650132ccb47436d2694a5b84a375ef414cd89ab3e26ac2a6f9f487e0b27a4`.
- Local changes: Downloaded mirror file unchanged.
- Imported geometry count: 12516 triangles across 22 mesh definitions; 13 materials.

## Mercedes-Benz AMG GT

- Original artwork: [Mercedes Benz AMG GT](https://sketchfab.com/3d-models/mercedes-benz-amg-gt-661dcab94455463784651a3ebc63cfb9).
- Author: [Yan Carvalho](https://sketchfab.com/carvalhoyan).
- License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- Import file: `mercedes-benz-amg-gt-import.glb`; SHA-256: `d90d23addd23e4212612e87c2937b4b2c11a5ccd43626c0fe250809250a33d8b`.
- [Exact mirrored download](https://raw.githubusercontent.com/pedrosoandre/carros/2d18dfd9db96bbf0d9cffa949deaf7f571388cbf/assets/mercedes-benz-amg-gt.glb); original downloaded file: `mercedes-benz-amg-gt.glb`; SHA-256: `fe15897ad73d7c87b4e864a4724ae90b8377f382be5b462b40a0d3fb0a787422`.
- Local changes: Decoded EXT_meshopt_compression and dequantized vertex attributes using @gltf-transform/cli 4.4.2; no intentional mesh simplification or styling changes. Original compressed mirror file retained.
- Imported geometry count: 166321 triangles across 33 mesh definitions; 30 materials.

## Processing and validation

The mirror describes normalizing and simplifying parts of its collection. Its source license explanation is preserved in `Provenance/mirror-LICENSE-3D-MODELS.md`. The files here are mirror derivatives, not represented as byte-identical to the original artist download. Embedded GLB author, source and license metadata agrees with the independently retrieved original metadata for all three models.

The AMG import derivative removes the need for the `EXT_meshopt_compression` and `KHR_mesh_quantization` extensions. Reproduce the derivative with:

```sh
npx --yes @gltf-transform/cli@4.4.2 dequantize mercedes-benz-amg-gt.glb mercedes-benz-amg-gt-import.glb
```

Validated all three designated import GLBs with `@gltf-transform/cli` version **4.4.2** using its `validate` command. Each returned **zero errors and zero warnings**. BMW and G-Class have informational unused-attribute notices. All buffers and images are embedded; no external texture downloads are needed. `asset_manifest.json` records byte counts, checksums, source URLs and import destinations.

## Mobile game models

The included Godot project uses three derived meshes in `assets/models`. For each car, `tools/bake_models.gd` rotates the scene to face forward, sets its length in metres, centres the geometry, and places the wheels at ground level. It batches parts by material and saves the result as a Godot mesh resource. The AMG conversion removes the display floor from the original scene. The supplied source GLBs retain their original attribution and provide inputs to the bake script.

The game has its own arcade handling parameters. The vehicle art and names do not imply manufacturer approval or sponsorship.
