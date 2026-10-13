#pragma once

#include <array>

#include "gta20/assets/gmp_map.hpp"
#include "gta20/core/types.hpp"
#include "gta20/scene/block_geometry.hpp"

namespace gta20::scene {

/// Corner of a tile image in texture space (u to the right, v increasing DOWN the image, so the
/// first pixel row is v = 0):
///
///   A = (u 0, v 0) top-left      B = (u 1, v 0) top-right
///   D = (u 0, v 1) bottom-left   C = (u 1, v 1) bottom-right
enum class TexCorner : u8 { A, B, C, D };

/// Texture label of each geometry corner p0..p3 of `face` (indexed exactly like
/// FaceGeometry::positions from block_face_geometry), for a face word's rotation and flip bits.
/// GPU-independent; nothing in the runtime draws with it yet.
///
/// Contract (docs/specs/world-geometry.md, sections 3 and 5):
/// - The face is viewed from outside: the lid from +z with +y down the screen, walls by an upright
///   viewer (head towards +z). With R0 and no flip the image's top-left..bottom-left corners A, B,
///   C, D land on the viewed top-left, top-right, bottom-right and bottom-left corners: p0..p3 get
///   A, B, C, D on the lid and C, D, A, B on every wall (walls list their lower corners first).
/// - flip mirrors the image horizontally in texture space (A<->B, D<->C) BEFORE the rotation.
/// - The rotation then turns the (possibly mirrored) image clockwise as viewed, by
///   `rotation` x 90 degrees.
/// - Valid rotations are the four enumerators R0..R270, i.e. the two-bit field produced by
///   SideFace/LidFace::decode. Only the low two bits of the underlying value are read.
///
/// Status: for the lid this reproduces what append_map_textured draws today (current-engine
/// regression behaviour). For walls it encodes the specification's hypothesis. Neither is
/// evidence of how the original GTA 2 orients textures.
[[nodiscard]] std::array<TexCorner, 4> face_texture_corners(BlockFace face, assets::Rotation rotation, bool flip);

} // namespace gta20::scene
