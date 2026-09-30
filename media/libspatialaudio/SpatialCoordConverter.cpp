// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

// PICO OS 5.13.7 libspatialaudio (spatial::SpatialCoordConverter), reconstructed from the
// factory /system/lib64/libspatialaudio.so. The factory file has no LOG_TAG.

#include <spatialaudio/SpatialCoordConverter.h>

#include <string.h>

#include <Eigen/Geometry>
#include <log/log.h>

namespace spatial {

namespace {

constexpr float kEpsilon = 1e-6f;

// Rotation whose columns are right (up x front), up and front; the identity when front and
// up point in the same direction.
Eigen::Matrix3f rotationFromFrontUp(Eigen::Vector3f front, const Eigen::Vector3f& up) {
    Eigen::Matrix3f rotation = Eigen::Matrix3f::Identity();
    if (front.cross(up).norm() < kEpsilon) {
        ALOGE("up vector and front vector are pointing in the same direction. "
              "using the identity transformation.");
        return rotation;
    }
    front.normalize();
    Eigen::Vector3f right = up.cross(front);
    right.normalize();
    const Eigen::Vector3f newUp = front.cross(right);
    rotation.col(0) = right;
    rotation.col(1) = newUp;
    rotation.col(2) = front;
    return rotation;
}

} // namespace

int SpatialCoordConverter::setCoordinateTransform(const float* transform) {
    if (transform == nullptr) {
        return -4;
    }
    Eigen::Vector3f x(transform[0], transform[1], transform[2]);
    Eigen::Vector3f y(transform[3], transform[4], transform[5]);
    Eigen::Vector3f z(transform[6], transform[7], transform[8]);
    if (x.dot(y) >= kEpsilon || x.dot(z) >= kEpsilon || y.dot(z) >= kEpsilon) {
        ALOGE("Not orthogonal coordinate system (%f %f %f)(%f %f %f)(%f %f %f)",
                x.x(), x.y(), x.z(), y.x(), y.y(), y.z(), z.x(), z.y(), z.z());
        return -4;
    }
    x.normalize();
    y.normalize();
    z.normalize();
    std::lock_guard<std::mutex> lock(mLock);
    Eigen::Matrix3f coordTransform;
    coordTransform << x, y, z;
    mCoordTransform = coordTransform;
    return 0;
}

int SpatialCoordConverter::setAdditionalCameraOrientation(float frontX, float frontY,
        float frontZ, float upX, float upY, float upZ) {
    Eigen::Vector3f front(frontX, frontY, frontZ);
    Eigen::Vector3f up(upX, upY, upZ);
    if (front.dot(up) >= kEpsilon) {
        ALOGE("%s Not orthogonal front an up vectors (%f %f %f)(%f %f %f)", __func__,
                frontX, frontY, frontZ, upX, upY, upZ);
        return -4;
    }
    std::lock_guard<std::mutex> lock(mLock);
    if (mCoordTransform.has_value()) {
        front = *mCoordTransform * front;
        up = *mCoordTransform * up;
    }
    mAdditionalCameraOrientation = rotationFromFrontUp(front, up);
    // Rotation of the camera to the audio coordinate system (180 degrees around y).
    Eigen::Matrix3f cameraToAudio;
    cameraToAudio << -1.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, -1.0f;
    mAdditionalCameraOrientation = mAdditionalCameraOrientation->inverse() * cameraToAudio;
    return 0;
}

void SpatialCoordConverter::clearAdditionalCameraOrientation() {
    std::lock_guard<std::mutex> lock(mLock);
    mAdditionalCameraOrientation.reset();
}

int SpatialCoordConverter::convertRelativeAudioOrientation(float targetX, float targetY,
        float targetZ, float reserved0 __unused, float reserved1 __unused,
        float reserved2 __unused, float upX, float upY, float upZ,
        float originX, float originY, float originZ, float* orientation, int size) {
    if (orientation == nullptr || size != 16) {
        return -4;
    }
    return convertAudioOrientation(targetX - originX, targetY - originY, targetZ - originZ,
            upX, upY, upZ, orientation, size);
}

int SpatialCoordConverter::convertAudioOrientation(float frontX, float frontY, float frontZ,
        float upX, float upY, float upZ, float* orientation, int size) {
    if (orientation == nullptr || size != 16) {
        return -4;
    }
    Eigen::Vector3f front(frontX, frontY, frontZ);
    Eigen::Vector3f up(upX, upY, upZ);
    const float dot = front.dot(up);
    if (dot > kEpsilon) {
        ALOGE("%s Not orthogonal front up (%f %f %f)(%f %f %f) dot %f", __func__,
                frontX, frontY, frontZ, upX, upY, upZ, dot);
        return -4;
    }
    std::lock_guard<std::mutex> lock(mLock);
    if (mCoordTransform.has_value()) {
        front = *mCoordTransform * front;
        up = *mCoordTransform * up;
    }
    Eigen::Matrix3f rotation = rotationFromFrontUp(front, up);
    if (mAdditionalCameraOrientation.has_value()) {
        rotation = *mAdditionalCameraOrientation * rotation;
    }
    const Eigen::Quaternionf quaternion(rotation);
    memcpy(orientation, quaternion.coeffs().data(), sizeof(float) * 4);
    return 0;
}

} // namespace spatial
