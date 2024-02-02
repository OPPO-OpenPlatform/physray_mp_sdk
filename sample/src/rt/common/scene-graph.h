/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rt-scene.h>
#include <ostream>
#include <list>
#include <queue>

// namespace of scene graph
namespace sg {

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
/// Call Transform::translation() to get its translation part, and Transform::rotation() for the rotation part.
class Transform : public Eigen::AffineCompact3f {
public:
    using Eigen::AffineCompact3f::operator=;

    /// default construction
    Transform() { *this = Identity(); }

    /// construct from float3x4
    Transform(const Eigen::Matrix<float, 3, 4> & f) {
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
    Transform(const Eigen::AffineCompact3f & t): Eigen::AffineCompact3f(t) {}

    /// copy construct from affine transform
    Transform(const Eigen::Affine3f & t): Eigen::AffineCompact3f(t) {}

    /// copy form affine transform
    Transform & operator=(const Eigen::AffineCompact3f & t) {
        *(Eigen::AffineCompact3f *) this = t;
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// comparison operators
    bool operator==(const Transform & rhs) const {
        if (this == &rhs) return true;
        const float * a = data();
        const float * b = rhs.data();
        for (size_t i = 0; i < 12; ++i, ++a, ++b) {
            if (*a != *b) return false;
        }
        return true;
    }
    bool operator!=(const Transform & rhs) const { return !(*this == rhs); }

    // -----------------------------------------------------------------------------------------------------------------
    /// Convenience function for setting transform from translation, rotation, and scaling.
    Transform & reset(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
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
    static inline Transform make(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
                                 const Eigen::Vector3f & s = Eigen::Vector3f::Ones()) {
        Transform tr;
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
    const Transform & decompose(Eigen::Vector3f * t, Eigen::Quaternionf * r, Eigen::Vector3f * s) const {
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
    Transform & setTranslation(float x, float y = 0.f, float z = 0.f) {
        Eigen::Vector3f    t(x, y, z);
        Eigen::Quaternionf r(rotation());
        Eigen::Vector3f    s(scaling());
        reset(t, r, s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    Transform & setRotation(const Eigen::Quaternionf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, r, s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    Transform & setRotation(const Eigen::AngleAxisf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, Eigen::Quaternionf(r), s);
        return *this;
    }

    // -----------------------------------------------------------------------------------------------------------------
    ///
    Transform & setScaling(const Eigen::Vector3f & s) {
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
    /// convert to 3x4 matrix
    operator Eigen::Matrix<float, 3, 4>() const {
        auto                       m = matrix();
        Eigen::Matrix<float, 3, 4> f;

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
    friend inline std::ostream & operator<<(std::ostream & os, const Transform & rhs) {
        os << rhs.matrix();
        return os;
    }
};

// /// By default node components can attach to multiple node.
// template<typename BASE>
// struct SingleNodePolicy {
//     static const bool value = false;
// };

// /// Light can only attached to at most one node at any given time.
// template<>
// struct SingleNodePolicy<Light> {
//     static const bool value = true;
// };

// template<typename BASE>
// class NodeComponentImpl : public RootImpl<BASE> {
// public:
//     NodeComponentImpl(const Root::RootConstructParameters & rcp): RootImpl<BASE>(rcp) {}

//     ~NodeComponentImpl() {}

//     ArrayView<Node *> nodes() const { return {_nodes.data(), _nodes.size()}; }

//     bool checkForDeletion() const { return _nodes.empty(); }

//     /// Add this NodeComponent to the Node.
//     bool onAttachedToNode(Node * n) {
//         PH_ASSERT(n);

//         // the new node should not be in the node list.
//         PH_ASSERT(_nodes.end() == std::find(_nodes.begin(), _nodes.end(), n));

//         if PH_RT_CONSTEXPR (SingleNodePolicy<BASE>::value) {
//             PH_ASSERT(_nodes.size() <= 1);
//             if (!_nodes.empty()) {
//                 PH_LOGW("Can't attach to new node. Must detach from old node first.");
//                 return false;
//             }
//         }

//         _nodes.push_back(n);

//         // done
//         return true;
//     }

//     void onDetachedFromNode(Node * n) {
//         PH_ASSERT(n);
//         auto iter = std::find(_nodes.begin(), _nodes.end(), n);
//         PH_ASSERT(_nodes.end() != iter); // must be a node that is already in the list.
//         _nodes.erase(iter);
//     }

// private:
//     std::vector<Node *> _nodes;
// };

class Graph;

/// Node implementation class.
class Node {
public:
    std::string name; ///< name of the node. For debugging and logging purpose only. Can be set to any value.

    auto graph() const -> const Graph & { return _graph; }
    auto graph() -> Graph & { return _graph; }
    auto scene() const -> ph::rt::Scene &;
    auto world() const -> ph::rt::World &;
    auto parent() const -> const Node * { return _parent; }
    auto parent() -> Node * { return _parent; }
    void setParent(Node *);
    auto children() const -> const std::list<Node *> & { return _children; }
    auto children() -> const std::list<Node *> & { return _children; }

    void detachAllComponents();

    auto attachComponent(ph::rt::Model *, uint32_t mask = 0xFF) -> int64_t; /// returns id of the entity. 0 if failed.
    void detachComponent(ph::rt::Model *);
    void detachAllModels();
    auto model() const { return _models.empty() ? nullptr : _models[0].first; }
    auto modelCount() const -> size_t { return _models.size(); }
    template<typename PROC>
    void forEachModel(PROC proc) {
        for (auto m : _models) proc(m.first, m.second);
    }

    void attachComponent(ph::rt::Light * light);
    void detachComponent(ph::rt::Light * light);
    void detachAllLights();
    auto light() const { return _lights.empty() ? nullptr : _lights[0].first; }
    auto lightEntity() const { return _lights.empty() ? 0 : _lights[0].second; }
    auto lightCount() const -> size_t { return _lights.size(); }
    template<typename PROC>
    void forEachLight(PROC proc) {
        for (auto l : _lights) proc(l.first, l.second);
    }

    auto transform() const -> const Transform & { return _local2Parent; }
    void setTransform(const Transform & transform);
    auto worldTransform() const -> const Transform & {
        updateWorldTransform();
        return _local2World;
    }
    void setWorldTransform(const Transform & worldToParent);
    void flushWorldTransform() { // flush the world transform matrix to scene entity.
        auto &                     s = scene();
        Eigen::Matrix<float, 3, 4> t = worldTransform();
        for (auto m : _models) s.setTransform(m.second, t);
        for (auto l : _lights) s.setTransform(l.second, t);
    }

    void setVisible(bool);

    /// traverse subtree of current code in BFS order.
    enum class TraverseAction {
        STOP = 0,
        CONTINUE,
        SKIP_SUBTREE,
    };
    template<typename PROC>
    bool bfsTraverseNodeGraph(PROC p) {
        std::queue<Node *> pending;
        pending.push(this);
        while (!pending.empty()) {
            // update the queue
            auto n = pending.front();
            pending.pop();

            TraverseAction a = p(n);

            if (TraverseAction::STOP == a)
                return false;
            else if (TraverseAction::CONTINUE == a) {
                for (auto c : n->children()) pending.push(c);
            }
        }
        return true;
    }

#ifndef UNIT_TEST
private:
#endif
    friend class Graph;

    Graph &                                           _graph;
    std::list<Node *>::iterator                       _iter; // where the node is in the graph.
    Node *                                            _parent = nullptr;
    std::list<Node *>                                 _children;
    std::vector<std::pair<ph::rt::Model *, uint64_t>> _models;
    std::vector<std::pair<ph::rt::Light *, uint64_t>> _lights;

    // The current local->world transform of the node.
    mutable Transform _local2World = Transform::Identity();

    // Used to record if the world transform needs to be updated.
    mutable bool _worldTransformDirty = true;

    // The current local->parent transform of the node.
    Transform _local2Parent = Transform::Identity();

private:
    Node(Graph & graph, Node * parent);

    ~Node();

    // Marks this and all descendants as needing to update
    void setWorldTransformDirty();

    // Updates the world transforms of this node and any dirty parents.
    void updateWorldTransform() const;

    // Updates this node's local to world transform to
    // be the combination of its parent's transform and its own
    // local to parent transform, then sets worldTransformDirty to false.
    // Update is based on a dirty flag for a get method,
    // hence why it's const.
    //
    // This method should ONLY be called from updateWorldTransform(),
    // else it will risk using data from out of date parents.
    void recalculateWorldTransform() const;
};

class Graph {
public:
    Graph(ph::rt::Scene & scene);
    ~Graph();

    auto world() const -> ph::rt::World & { return *_scene.world(); }
    auto scene() const -> ph::rt::Scene & { return _scene; }
    auto root() const -> const Node & { return _root; }
    auto root() -> Node & { return _root; }
    auto createNode(Node * parent = nullptr) -> Node *;
    void deleteNodeAndSubtree(Node *& node);
    void refreshSceneGpuData(VkCommandBuffer); // update the scene with the latest transformation matrices.

private:
    ph::rt::Scene &   _scene;
    Node              _root {*this, nullptr};
    std::list<Node *> _nodes;
};

inline ph::rt::Scene & Node::scene() const { return _graph.scene(); }
inline ph::rt::World & Node::world() const { return _graph.world(); }

} // namespace sg
