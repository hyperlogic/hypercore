/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <memory>
#include <string>

namespace hyper {

class Asset;

// Loads a pickled poselib SkeletonState from a .npy file (as written by
// numpy.save(filename, np.array(skeleton_state_dict, dtype=object), allow_pickle=True)).
// Only single frame SkeletonState dicts are supported.  The resulting Asset contains
// only a node hierarchy (no meshes, animations or textures).
// On failure an empty Asset is returned.
std::shared_ptr<Asset> AssetImportNpyAbs(const std::string& filename);

}  // namespace hyper
