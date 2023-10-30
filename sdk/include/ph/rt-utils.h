/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/va.h>
#include "rt.h"
#include <ostream>

namespace ph {
namespace rt {

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Vector2f toEigen(const Float2 & f) {
    Eigen::Vector2f v;
    v.x() = f.x;
    v.y() = f.y;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Float2 fromEigen(const Eigen::Vector2f & v) {
    Float2 f;
    f.x = v.x();
    f.y = v.y();
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Vector3f toEigen(const Float3 & f) {
    Eigen::Vector3f v;
    v.x() = f.x;
    v.y() = f.y;
    v.z() = f.z;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Float3 fromEigen(const Eigen::Vector3f & v) {
    Float3 f;
    f.x = v.x();
    f.y = v.y();
    f.z = v.z();
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Vector4f toEigen(const Float4 & f) {
    Eigen::Vector4f v;
    v.x() = f.x;
    v.y() = f.y;
    v.z() = f.z;
    v.w() = f.w;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Matrix3f toEigen(const Float3x3 & f) {
    Eigen::Matrix3f m;
    m << f(0, 0), f(0, 1), f(0, 2), f(1, 0), f(1, 1), f(1, 2), f(2, 0), f(2, 1), f(2, 2);
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Float3x3 fromEigen(const Eigen::Matrix3f & m) {
    Float3x3 f;
    f(0, 0) = m(0, 0);
    f(0, 1) = m(0, 1);
    f(0, 2) = m(0, 2);
    f(1, 0) = m(1, 0);
    f(1, 1) = m(1, 1);
    f(1, 2) = m(1, 2);
    f(2, 0) = m(2, 0);
    f(2, 1) = m(2, 1);
    f(2, 2) = m(2, 2);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Matrix4f toEigen(const Float3x4 & f) {
    Eigen::Matrix4f m;
    m(0, 0) = f(0, 0);
    m(0, 1) = f(0, 1);
    m(0, 2) = f(0, 2);
    m(0, 3) = f(0, 3);
    m(1, 0) = f(1, 0);
    m(1, 1) = f(1, 1);
    m(1, 2) = f(1, 2);
    m(1, 3) = f(1, 3);
    m(2, 0) = f(2, 0);
    m(2, 1) = f(2, 1);
    m(2, 2) = f(2, 2);
    m(2, 3) = f(2, 3);
    m(3, 0) = 0.0f;
    m(3, 1) = 0.0f;
    m(3, 2) = 0.0f;
    m(3, 3) = 1.0f;
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Matrix4f toEigen(const Float4x4 & f) {
    Eigen::Matrix4f m;
    m(0, 0) = f(0, 0);
    m(0, 1) = f(0, 1);
    m(0, 2) = f(0, 2);
    m(0, 3) = f(0, 3);
    m(1, 0) = f(1, 0);
    m(1, 1) = f(1, 1);
    m(1, 2) = f(1, 2);
    m(1, 3) = f(1, 3);
    m(2, 0) = f(2, 0);
    m(2, 1) = f(2, 1);
    m(2, 2) = f(2, 2);
    m(2, 3) = f(2, 3);
    m(3, 0) = f(3, 0);
    m(3, 1) = f(3, 1);
    m(3, 2) = f(3, 2);
    m(3, 3) = f(3, 3);
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline Float4x4 fromEigen(const Eigen::Matrix4f & m) {
    Float4x4 f;
    f(0, 0) = m(0, 0);
    f(0, 1) = m(0, 1);
    f(0, 2) = m(0, 2);
    f(0, 3) = m(0, 3);
    f(1, 0) = m(1, 0);
    f(1, 1) = m(1, 1);
    f(1, 2) = m(1, 2);
    f(1, 3) = m(1, 3);
    f(2, 0) = m(2, 0);
    f(2, 1) = m(2, 1);
    f(2, 2) = m(2, 2);
    f(2, 3) = m(2, 3);
    f(3, 0) = m(3, 0);
    f(3, 1) = m(3, 1);
    f(3, 2) = m(3, 2);
    f(3, 3) = m(3, 3);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Defines location and orientation of an object in it's parent coordinate system.
///
/// It is based on right handed coordinate system:
///
///     +X -> right
///     +Y -> top
///     +Z -> inward (pointing out of screen, to your eyes)
///
/// It can transform vector from local space to parent space.
///
/// Call NodeTransform::translation() to get its translation part, and NodeTransform::rotation() for the rotation part.
class NodeTransform : public Eigen::AffineCompact3f {
public:
    using Eigen::AffineCompact3f::operator=;

    /// default construction
    NodeTransform() { *this = Identity(); }

    /// construct from float3x4
    NodeTransform(const Float3x4 & f) {
        auto & m = matrix();
        m(0, 0)  = f(0, 0);
        m(0, 1)  = f(0, 1);
        m(0, 2)  = f(0, 2);
        m(0, 3)  = f(0, 3);

        m(1, 0) = f(1, 0);
        m(1, 1) = f(1, 1);
        m(1, 2) = f(1, 2);
        m(1, 3) = f(1, 3);

        m(2, 0) = f(2, 0);
        m(2, 1) = f(2, 1);
        m(2, 2) = f(2, 2);
        m(2, 3) = f(2, 3);
    }

    /// copy construct from affine transform
    NodeTransform(const Eigen::AffineCompact3f & t): Eigen::AffineCompact3f(t) {}

    /// copy form affine transform
    NodeTransform & operator=(const Eigen::AffineCompact3f & t) {
        *(Eigen::AffineCompact3f *) this = t;
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// comparison operators
    bool operator==(const NodeTransform & rhs) const {
        if (this == &rhs) return true;
        const float * a = data();
        const float * b = rhs.data();
        for (size_t i = 0; i < 12; ++i, ++a, ++b) {
            if (*a != *b) return false;
        }
        return true;
    }
    bool operator!=(const NodeTransform & rhs) const { return !(*this == rhs); }

    // -----------------------------------------------------------------------------------------------------------------
    /// Convenience function for setting transform from translation, rotation, and scaling.
    NodeTransform & reset(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
                          const Eigen::Vector3f & s = Eigen::Vector3f::Ones()) {
        // Make sure the transform is initialized to identity.
        *this = Identity();

        // Combine everything into the transform by order of translate, rotate, scale.
        translate(t);
        rotate(r);
        scale(s);

        // Done.
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// Convenience function for setting transform from
    /// translation, rotation, and scaling.
    static inline NodeTransform make(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
                                     const Eigen::Vector3f & s = Eigen::Vector3f::Ones()) {
        NodeTransform tr;
        tr.reset(t, r, s);
        return tr;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// Get the scaling part of the transformation
    Eigen::Vector3f scaling() const {
        // Fetch the rotation scaling.
        Eigen::Matrix3f scaling;
        computeRotationScaling((Eigen::Matrix3f *) nullptr, &scaling);

        // Convert scale to vector and return it.
        return Eigen::Vector3f {scaling(0, 0), scaling(1, 1), scaling(2, 2)};
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// Decomposes the transform into its translation, rotation, and scale.
    /// Will ignore any null pointers.
    const NodeTransform & decompose(Eigen::Vector3f * t, Eigen::Quaternionf * r, Eigen::Vector3f * s) const {
        // Fetch the translation.
        if (t != nullptr) { *t = translation(); }

        // Fetch the rotation scaling.
        // If user requested rotation.
        if (r != nullptr) {
            // If user requested rotation and scale.
            if (s != nullptr) {
                // Compute rotation and scaling.
                Eigen::Matrix3f rotationMatrix;
                Eigen::Matrix3f scalingMatrix;
                computeRotationScaling(&rotationMatrix, &scalingMatrix);
                *r = rotationMatrix;
                *s = Eigen::Vector3f {scalingMatrix(0, 0), scalingMatrix(1, 1), scalingMatrix(2, 2)};

                // If user only requested rotation.
            } else {
                *r = rotation();
            }

            // If user requested scale and not rotation.
        } else if (s != nullptr) {
            *s = scaling();
        }
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    NodeTransform & setTranslation(float x, float y = 0.f, float z = 0.f) {
        Eigen::Vector3f    t(x, y, z);
        Eigen::Quaternionf r(rotation());
        Eigen::Vector3f    s(scaling());
        reset(t, r, s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    NodeTransform & setRotation(const Eigen::Quaternionf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, r, s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    NodeTransform & setRotation(const Eigen::AngleAxisf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, Eigen::Quaternionf(r), s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    ///
    NodeTransform & setScaling(const Eigen::Vector3f & s) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f    t(translation());
        Eigen::Quaternionf r(rotation());
        reset(t, r, s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// convert to 4x4 matrix
    void toMatrix4f(Eigen::Matrix4f & m) const {
        m.block<3, 4>(0, 0) = this->matrix();
        m.block<1, 4>(3, 0) = Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // ---------------------------------------------------------------------------------------------------------------------
    /// convert to 4x4 matrix
    Eigen::Matrix4f matrix4f() const {
        Eigen::Matrix4f m;
        toMatrix4f(m);
        return m;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// convert to 4x4 matrix
    operator Float3x4() const {
        auto     m = matrix();
        Float3x4 f;

        f(0, 0) = m(0, 0);
        f(0, 1) = m(0, 1);
        f(0, 2) = m(0, 2);
        f(0, 3) = m(0, 3);

        f(1, 0) = m(1, 0);
        f(1, 1) = m(1, 1);
        f(1, 2) = m(1, 2);
        f(1, 3) = m(1, 3);

        f(2, 0) = m(2, 0);
        f(2, 1) = m(2, 1);
        f(2, 2) = m(2, 2);
        f(2, 3) = m(2, 3);

        return f;
    }

    // -----------------------------------------------------------------------------------------------------------------
    friend inline std::ostream & operator<<(std::ostream & os, const NodeTransform & rhs) {
        os << rhs.matrix();
        return os;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to append shadow mode to the end of a stream.
inline std::ostream & operator<<(std::ostream & os, RayTracingRenderPack::ShadowMode mode) {
    switch (mode) {
    case RayTracingRenderPack::RAY_TRACED:
        os << "RAY_TRACED(";
        break;
    case RayTracingRenderPack::RASTERIZED:
        os << "RASTERIZED(";
        break;
    case RayTracingRenderPack::REFINED:
        os << "REFINED   (";
        break;
    case RayTracingRenderPack::DEBUG:
        os << "DEBUG     (";
        break;
    default:
        os << "(";
        break;
    }
    os << (int) mode << ")";
    return os;
}

/// convert ImageObject to TextureHandle
inline Material::TextureHandle toTextureHandle(const va::ImageObject & image) { return Material::TextureHandle(image); }

/// A utility function to setup SimpleVulkanDevice::ConstructParameters to for ray tracing
/// \param hw Whether to use hardware VK_KHR_ray_query extension. If set to false, then setup the construct
///           parameter for in-house compute shader based pipeline. In this case, return value is always false.
/// \return   If the construction parameter is properly set for hardware ray query. If false is returned,
///           then construction parameter structure is set to do in-house shader based pipeline.
bool setupDeviceConstructionForRayQuery(va::SimpleVulkanDevice::ConstructParameters &, bool hw);

} // namespace rt
} // namespace ph

namespace std {

/// A functor that allows for hashing texture handles.
template<>
struct hash<ph::rt::Material::TextureHandle> {
    size_t operator()(const ph::rt::Material::TextureHandle & key) const {
        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        hash = 79 * hash + (size_t) key.image;
        hash = 79 * hash + (size_t) key.view;

        return hash;
    }
};

/// A functor that allows for hashing material descriptions.
template<>
struct hash<ph::rt::Material::Desc> {
    size_t operator()(const ph::rt::Material::Desc & key) const {
        // Make sure floats will fit inside the integral type we are copying the unmodified bytes to.
        // If on a platform that defines it differently than we expect, this method will need to be rewritten.
        static_assert(sizeof(size_t) >= sizeof(float), "");

        // Make sure albedo is at the very beginning of the descriptor since our float iteration won't work otherwise.
        static_assert(0 == static_cast<size_t>(offsetof(ph::rt::Material::Desc, albedo)), "");

        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        // Hash the float members first.
        // Get a pointer to the first float variable in the struct.
        const float * p1 = key.albedo;

        // Calculate how many floats there are in the struct between albedo and maps.
        size_t count = static_cast<size_t>(offsetof(ph::rt::Material::Desc, maps) / sizeof(float));

        // Iterate over all floats in the struct before maps.
        for (size_t i = 0; i < count; ++i) {
            // Copy the float bits to size_t. Anything the float doesn't take up will default to zero.
            size_t value = 0;
            memcpy(&value, p1 + i, sizeof(float));

            hash = 79 * hash + value;
        }

        // Used to calculate the hash of the texture handles.
        std::hash<ph::rt::Material::TextureHandle> textureHandleHasher;

        // Iterate all the textures in this material.
        for (size_t i = 0; i < countOf(key.maps); ++i) {
            // Add the hash of each texture.
            hash = 79 * hash + textureHandleHasher(key.maps[i]);
        }

        return hash;
    }
};

} // namespace std