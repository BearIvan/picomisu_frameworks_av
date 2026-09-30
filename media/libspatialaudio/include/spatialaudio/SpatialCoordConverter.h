// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef SPATIALAUDIO_SPATIALCOORDCONVERTER_H
#define SPATIALAUDIO_SPATIALCOORDCONVERTER_H

#include <mutex>
#include <optional>

#include <Eigen/Dense>

namespace spatial {

// PICO OS 5.13.7 libspatialaudio: converts the orientation of a sound source, given as front
// and up vectors in the coordinate system of an engine (android.media.SpatialCoordConverter:
// COORD_TRANSFORM_OPENXR/UNITY/UNREAL), to the rotation quaternion (x, y, z, w) of the
// spatializer, optionally relative to an additional camera orientation.
// Results: 0 on success, -4 for invalid parameters.
class SpatialCoordConverter {
public:
    // |transform|: 3x3 column-major matrix with orthogonal columns (the engine's axes).
    int setCoordinateTransform(const float* transform);
    int setAdditionalCameraOrientation(float frontX, float frontY, float frontZ,
            float upX, float upY, float upZ);
    void clearAdditionalCameraOrientation();
    // |orientation|: 4 floats (x, y, z, w), |size| its size in bytes (16).
    // The front vector is target - origin; the three floats after the target are not used.
    int convertRelativeAudioOrientation(float targetX, float targetY, float targetZ,
            float reserved0, float reserved1, float reserved2,
            float upX, float upY, float upZ,
            float originX, float originY, float originZ,
            float* orientation, int size);
    int convertAudioOrientation(float frontX, float frontY, float frontZ,
            float upX, float upY, float upZ, float* orientation, int size);

private:
    std::mutex mLock;
    std::optional<Eigen::Matrix3f> mCoordTransform;
    std::optional<Eigen::Matrix3f> mAdditionalCameraOrientation;
};

} // namespace spatial

#endif // SPATIALAUDIO_SPATIALCOORDCONVERTER_H
