/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : culling.h
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include <queue>

struct CullingAlgorithm {
    std::string name;

    // default guid for get BBox from model
    Guid guidBBOX    = Guid::make(0x0, 0x0);
    Guid guidHasSkin = Guid::make(0x0, 0x1);

    // index for culling algorithm, used to switch between different algorithms
    float distanceCullingSize = 4.0f;

    virtual ~CullingAlgorithm() = default;

    // culling implementation
    virtual void culling(Node * node, const Eigen::Vector3f * camPos, const Eigen::Matrix4f & mvp) = 0;

    // calculate BBox transfer from local to world space
    // same function as the calculateWorldSpaceBoundingBox in gltf-scene-asset-builder
    static Eigen::AlignedBox3f calculateWorldSpaceBoundingBox(const ph::rt::NodeTransform & transform, const Eigen::AlignedBox3f & bbox) {
        // Total number of corners in a box.
        constexpr size_t cornerCount = 8;

        // Grab the coordinates of the 8 corners of the bounding box.
        Eigen::Vector3f corners[cornerCount] = {
            bbox.corner(Eigen::AlignedBox3f::CornerType::BottomLeftFloor), bbox.corner(Eigen::AlignedBox3f::CornerType::BottomRightFloor),
            bbox.corner(Eigen::AlignedBox3f::CornerType::TopLeftFloor),    bbox.corner(Eigen::AlignedBox3f::CornerType::TopRightFloor),
            bbox.corner(Eigen::AlignedBox3f::CornerType::BottomLeftCeil),  bbox.corner(Eigen::AlignedBox3f::CornerType::BottomRightCeil),
            bbox.corner(Eigen::AlignedBox3f::CornerType::TopLeftCeil),     bbox.corner(Eigen::AlignedBox3f::CornerType::TopRightCeil)};

        // Transform the corners individually.
        for (std::size_t index = 0; index < cornerCount; ++index) { corners[index] = transform * corners[index]; }

        // Use the transformed corners to calculate the transformed box.
        // Initialize it with the first corner.
        Eigen::AlignedBox3f result;
        result.min() = corners[0];
        result.max() = corners[0];

        // Extend the box by the combination of all corners
        // beyond the first (which the box has already been initialized to).
        for (std::size_t index = 1; index < cornerCount; ++index) { result.extend(corners[index]); }

        // done
        return result;
    }

    //     7-------6
    //   / |     / |
    //  3-------2  |
    //  |  |    |  |
    //  |  4----|--5
    //  | /     | /
    //  0-------1
    static std::vector<Eigen::Vector3f> calculateFrustumCorners(const Eigen::Matrix4f & mvp) {

        Eigen::Matrix4f inverseMVP = mvp.inverse();

        std::vector<Eigen::Vector3f> cornersBeforeMVP;
        std::vector<Eigen::Vector3f> cornersAfterMVP = {Eigen::Vector3f(-1.f, -1.f, -1.f), Eigen::Vector3f(1.f, -1.f, -1.f), Eigen::Vector3f(1.f, -1.f, 1.f),
                                                        Eigen::Vector3f(-1.f, -1.f, 1.f),  Eigen::Vector3f(-1.f, 1.f, -1.f), Eigen::Vector3f(1.f, 1.f, -1.f),
                                                        Eigen::Vector3f(1.f, 1.f, 1.f),    Eigen::Vector3f(-1.f, 1.f, 1.f)};

        for (int i = 0; i < 8; i++) {
            Eigen::Vector4f corner = inverseMVP * Eigen::Vector4f(cornersAfterMVP[i].x(), cornersAfterMVP[i].y(), cornersAfterMVP[i].z(), 1);
            cornersBeforeMVP.emplace_back(Eigen::Vector3f(corner.x() / corner.w(), corner.y() / corner.w(), corner.z() / corner.w()));
        }

        return cornersBeforeMVP;
    }

    static std::vector<Eigen::Hyperplane<float, 3>> calculateFrustumPlanes(const Eigen::Matrix4f & mvp) {

        std::vector<Eigen::Hyperplane<float, 3>> planes;
        std::vector<Eigen::Vector3f>             corners      = calculateFrustumCorners(mvp);
        std::vector<Eigen::Vector4i>             surfaceIndex = {
            Eigen::Vector4i(3, 2, 1, 0), // front
            Eigen::Vector4i(2, 6, 5, 1), // right
            Eigen::Vector4i(1, 5, 4, 0), // bottom
            Eigen::Vector4i(3, 7, 6, 2), // top
            Eigen::Vector4i(4, 7, 3, 0), // left
            Eigen::Vector4i(5, 6, 7, 4)  // back
        };

        for (int i = 0; i < 6; i++) {

            Eigen::Vector3f p0 = corners[surfaceIndex[i].x()];
            Eigen::Vector3f p1 = corners[surfaceIndex[i].y()];
            Eigen::Vector3f p2 = corners[surfaceIndex[i].z()];

            // construct hyperplane by 3 points (it is strange that using "Through()" function will cause problem)
            // Eigen::Hyperplane<float,3> plane = Eigen::Hyperplane<float,3>::Through(p1_ok,p2_ok,p3_ok);
            Eigen::Hyperplane<float, 3> plane;
            plane.normal() = (p2 - p0).cross(p1 - p0).normalized();
            plane.offset() = -p0.dot(plane.normal());

            planes.emplace_back(plane);
        }
        return planes;
    }

    static bool boundingSphereinstersectFrustum(const Eigen::Matrix4f & mvp, Eigen::Vector3f center, float radius) {
        std::vector<Eigen::Hyperplane<float, 3>> planes = calculateFrustumPlanes(mvp);

        for (int i = 0; i < 6; i++) {
            auto distance = planes[i].absDistance(center);
            if (distance < radius) { return true; }
        }
        return false;
    }

    static bool pointInsideFrustum(const Eigen::Matrix4f & mvp, Eigen::Vector3f center) {
        Eigen::Vector4f centerAfterMVP = mvp * Eigen::Vector4f(center.x(), center.y(), center.z(), 1);

        if (centerAfterMVP.x() / centerAfterMVP.w() <= 1 && centerAfterMVP.x() / centerAfterMVP.w() >= -1 && centerAfterMVP.y() / centerAfterMVP.w() <= 1 &&
            centerAfterMVP.y() / centerAfterMVP.w() >= -1 && centerAfterMVP.z() / centerAfterMVP.w() <= 1 && centerAfterMVP.z() / centerAfterMVP.w() >= -1) {
            return true;
        } else {
            return false;
        }
    }

    static bool boundingSphereIntersectOrInsideFrustum(const Eigen::Matrix4f & mvp, Eigen::Vector3f center, float radius) {
        std::vector<Eigen::Hyperplane<float, 3>> planes = calculateFrustumPlanes(mvp);

        for (int i = 0; i < 6; i++) {
            // negative value means inside frustum
            auto distance = planes[i].signedDistance(center);

            // if any distance is larger than radius, means complete outside
            if (distance > radius) { return false; }
        }

        return true;
    }

    static bool boundingBoxIntersectOrInsideFrustum(const Eigen::Matrix4f & mvp, Eigen::Vector3f center, Eigen::Vector3f absExtent) {
        std::vector<Eigen::Hyperplane<float, 3>> planes = calculateFrustumPlanes(mvp);

        for (int i = 0; i < 6; i++) {
            Eigen::Vector3f planeNormal = planes[i].normal();

            // Calculate the distance (x * x) + (y * y) + (z * z) + w
            auto distance = planes[i].signedDistance(center);

            // push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
            float pushOut = abs(absExtent.x() * planeNormal.x()) + abs(absExtent.y() * planeNormal.y()) + abs(absExtent.z() * planeNormal.z());

            if (distance > pushOut) { return false; }
        }

        return true;
    }
};

// no culling, set all instance visible
struct EmptyCullingAlgorithm : CullingAlgorithm {

    EmptyCullingAlgorithm() { name = "Culling Disabled"; }

    void culling(Node * node, const Eigen::Vector3f * camPos, const Eigen::Matrix4f & mvp) override {
        (void) camPos;
        (void) mvp;
        int index = 0;
        for (auto c : node->components()) {
            if (c->type() == NodeComponent::MODEL) { node->setComponentVisible(index, true); }
            index++;
        }
    }
};

// distance culling, check if (distance between camera & model - visible distance) with ( model bounding sphere radius )
struct DistanceCullingAlgorithm : CullingAlgorithm {

    DistanceCullingAlgorithm() { name = "Distance Culling"; }

    void culling(Node * node, const Eigen::Vector3f * camPos, const Eigen::Matrix4f & mvp) override {
        (void) mvp;
        if (!node) { return; }
        int index = 0;
        for (auto c : node->components()) {
            if (c->type() == NodeComponent::MODEL) {

                // return if model do not contain any bbox data
                if (((Model *) c)->userData(guidBBOX).size() == 0) { return; }

                Eigen::AlignedBox3f modelBBox    = *(Eigen::AlignedBox3f *) (((Model *) c)->userData(guidBBOX).data());
                Eigen::AlignedBox3f instanceBBox = calculateWorldSpaceBoundingBox(node->worldTransform(), modelBBox);

                Eigen::Vector3f instanceCenter    = instanceBBox.center();
                Eigen::Vector3f radiusVector      = (instanceBBox.max() - instanceBBox.min()) / 2.0;
                Eigen::Vector3f camInstanceVector = instanceCenter - *camPos;

                // TODO: this method calculate square root, could remove this and compare square
                float radius          = radiusVector.norm();
                float camInstanceDiff = camInstanceVector.norm();

                if (camInstanceDiff < radius + distanceCullingSize) {
                    node->setComponentVisible(index, true);
                } else {
                    node->setComponentVisible(index, false);
                }
            }
            index++;
        }
    }
};

// Frustum Culling1, check if instance center is inside Frustum; this may cause incorrect culling
struct FrustumCullingAlgorithm1 : CullingAlgorithm {

    FrustumCullingAlgorithm1() { name = "Frustum Culling"; }

    void culling(Node * node, const Eigen::Vector3f * camPos, const Eigen::Matrix4f & mvp) {
        (void) camPos;
        if (!node) { return; }
        int index = 0;
        for (auto c : node->components()) {
            if (c->type() == NodeComponent::MODEL) {

                // return if model do not contain any bbox data
                if (((Model *) c)->userData(guidBBOX).size() == 0) { return; }

                Eigen::AlignedBox3f modelBBox    = *(Eigen::AlignedBox3f *) (((Model *) c)->userData(guidBBOX).data());
                Eigen::AlignedBox3f instanceBBox = calculateWorldSpaceBoundingBox(node->worldTransform(), modelBBox);

                // check instance center only
                Eigen::Vector3f instanceCenter = instanceBBox.center();
                Eigen::Vector3f instanceExtent = (instanceBBox.max() - instanceBBox.min()) / 2.0f;

                node->setComponentVisible(index, boundingBoxIntersectOrInsideFrustum(mvp, instanceCenter, instanceExtent));
            }
            index++;
        }
    }
};

// using bbox for static object and bounding sphere for animation(skinned mesh)
struct WarZoneCullingAlgorithm2 : CullingAlgorithm {

    WarZoneCullingAlgorithm2() { name = "WarZone Culling V2"; }

    void culling(Node * node, const Eigen::Vector3f * camPos, const Eigen::Matrix4f & mvp) {
        if (!node) { return; }
        int index = 0;
        for (auto c : node->components()) {
            if (c->type() == NodeComponent::MODEL) {

                // return direct if model do not contain any bbox data
                if (((Model *) c)->userData(guidHasSkin).size() == 0) { return; }

                bool hasSkin = *(bool *) (((Model *) c)->userData(guidHasSkin).data());

                Eigen::AlignedBox3f modelBBox      = *(Eigen::AlignedBox3f *) (((Model *) c)->userData(guidBBOX).data());
                Eigen::AlignedBox3f instanceBBox   = calculateWorldSpaceBoundingBox(node->worldTransform(), modelBBox);
                Eigen::Vector3f     instanceCenter = instanceBBox.center();
                Eigen::Vector3f     radiusVector   = (instanceBBox.max() - instanceBBox.min()) / 2.0;
                float               radius         = radiusVector.norm();

                bool instanceVisible = false;

                // Visible test1: distance culling
                {
                    Eigen::Vector3f camInstanceVector = instanceCenter - *camPos;
                    float           camInstanceDiff   = camInstanceVector.norm();

                    if (camInstanceDiff < radius + distanceCullingSize) { instanceVisible = true; }
                }
                if (instanceVisible) {
                    node->setComponentVisible(index, true);
                    continue;
                }

                // Visible test2: frustum culling for all corners
                {
                    if (hasSkin) {
                        // check bounding sphere for dynmaic instance
                        instanceVisible = boundingSphereIntersectOrInsideFrustum(mvp, instanceCenter, radius);
                    } else {
                        // check bounding box for static instance
                        instanceVisible = boundingBoxIntersectOrInsideFrustum(mvp, instanceCenter, radiusVector);
                    }
                }
                if (instanceVisible) {
                    node->setComponentVisible(index, true);
                } else {
                    node->setComponentVisible(index, false);
                }
            }
            index++;
        }
    }
};

class CullingManager {
    std::vector<std::unique_ptr<CullingAlgorithm>> _algorithms;
    size_t                                         _activeAlgorithm = 0;
    ph::rt::Scene *                                _scene;
    Eigen::Vector3f                                _cameraPosition = Eigen::Vector3f(0.f, 0.f, 0.f);
    Eigen::Matrix4f                                _projView;

public:
    CullingManager() {
        _algorithms.emplace_back(new EmptyCullingAlgorithm());
        _algorithms.emplace_back(new DistanceCullingAlgorithm());
        _algorithms.emplace_back(new FrustumCullingAlgorithm1());
        _algorithms.emplace_back(new WarZoneCullingAlgorithm2());
    }

    void setCamera(Camera * cam, float displayW, float displayH) {
        Eigen::Affine3f viewTransform = cam->worldTransform().inverse();
        _cameraPosition               = viewTransform.inverse().translation();

        Eigen::Matrix4f viewMatrix = viewTransform.matrix();
        Eigen::Matrix4f proj       = cam->calculateProj(displayW, displayH);
        _projView                  = proj * viewMatrix;
    }

    void setScene(Scene * scene) { _scene = scene; }

    size_t numAlgorithms() const { return _algorithms.size(); }

    CullingAlgorithm & algorithm(size_t i) const { return *_algorithms[i].get(); }

    size_t activeAlgorithm() const { return _activeAlgorithm; }

    void setActiveAlgorithm(size_t index) { _activeAlgorithm = index; }

    float & cullingDistance() const { return _algorithms[_activeAlgorithm]->distanceCullingSize; }

    void update() {
        ph::rt::Node & rootNode = _scene->rootNode();
        auto           c        = _algorithms[_activeAlgorithm].get();
        bfsTraverseNodeGraph(&rootNode, [&](Node * n) { c->culling(n, &_cameraPosition, _projView); });
    }

private:
    template<typename PROC>
    void bfsTraverseNodeGraph(Node * root, PROC p) {
        std::queue<Node *> pending;
        pending.push(root);
        while (!pending.empty()) {
            // update the queue
            auto n = pending.front();
            pending.pop();
            p(n);

            for (auto c : n->children()) pending.push(c);
        }
    }
};