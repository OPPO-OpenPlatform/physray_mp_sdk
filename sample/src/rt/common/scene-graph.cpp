/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "scene-graph.h"

#include <queue>
#include <stack>

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

namespace sg {

// ---------------------------------------------------------------------------------------------------------------------
//
Node::Node(Graph & graph, Node * parent): _graph(graph), _parent(parent) {
    if (_parent) {
        // the parent node must be in the same scene.
        if (&_parent->graph() != &graph) PH_THROW("The parent node belongs to another graph.");
    } else if (this != &graph.root()) {
        // Default to scene root if user doesn't set a parent.
        _parent = (Node *) &graph.root();
    }

    // Record this as a child in its parent.
    if (_parent) {
        PH_ASSERT(_parent->_children.end() == std::find(_parent->_children.begin(), _parent->_children.end(), this));
        _parent->_children.push_back(this);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
Node::~Node() {
    // remove from parent's children list.
    if (_parent) {
        PH_ASSERT(1 == std::count(_parent->_children.begin(), _parent->_children.end(), this));
        _parent->_children.remove(this);
    }

    detachAllComponents();
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::setParent(Node * parent) {
    if (this == &_graph.root()) {
        if (parent) PH_LOGE("Can't set parent of root node.");
        return;
    }
    if (parent) {
        if (parent == _parent) return;
        if (parent == this) {
            PH_LOGE("can't set a node as its own parent.");
            return;
        }
        if (&parent->scene() != &scene()) {
            PH_LOGE("the new parent belongs to different scene.");
            return;
        }
        if (!bfsTraverseNodeGraph([&](Node * n) { return n == parent ? TraverseAction::STOP : TraverseAction::CONTINUE; })) {
            PH_LOGE("Can't set descendant node as parent.");
            return;
        }
    } else {
        parent = &_graph.root();
        if (parent == _parent) return;
    }

    PH_ASSERT(parent != _parent);

    // remove from old parent's children list.
    auto iter = std::find(_parent->_children.begin(), _parent->_children.end(), this);
    PH_ASSERT(iter != _parent->_children.end());
    _parent->_children.erase(iter);

    // add to new parent
    _parent = (Node *) parent;
    PH_ASSERT(_parent->_children.end() == std::find(_parent->_children.begin(), _parent->_children.end(), this));
    _parent->_children.push_back(this);

    // mark world transform as dirty
    setWorldTransformDirty();
}

// ---------------------------------------------------------------------------------------------------------------------
//
int64_t Node::attachComponent(Model * m, uint32_t mask) {
    if (!m) return 0;
    auto iter = std::find_if(_models.begin(), _models.end(), [&](auto & p) { return p.first == m; });
    if (iter != _models.end()) {
        PH_LOGW("ignore redundant model: %s", m->name.data());
        return 0;
    }
    auto entity = scene().addModel(*m, mask);
    if (entity) _models.push_back({m, entity});
    return entity;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::detachComponent(Model * m) {
    if (!m) return;
    auto iter = std::find_if(_models.begin(), _models.end(), [&](auto & p) { return p.first == m; });
    if (iter == _models.end()) {
        PH_LOGW("ignore model that is not attached to current node: %s", m->name.data());
        return;
    }
    PH_ASSERT(iter->second);
    scene().deleteEntity(iter->second);
    _models.erase(iter);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::detachAllModels() {
    for (auto & m : _models) scene().deleteEntity(m.second);
    _models.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::attachComponent(Light * l) {
    if (!l) return;
    auto iter = std::find_if(_lights.begin(), _lights.end(), [&](auto & p) { return p.first == l; });
    if (iter != _lights.end()) {
        PH_LOGW("ignore redundant light: %s", l->name.data());
        return;
    }
    auto entity = scene().addLight(*l);
    if (!entity) return;
    _lights.push_back({l, entity});
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::detachComponent(Light * l) {
    if (!l) return;
    auto iter = std::find_if(_lights.begin(), _lights.end(), [&](auto & p) { return p.first == l; });
    if (iter == _lights.end()) {
        PH_LOGW("can't detach light (%s) that is not attached to node (%s)", l->name.data(), name.data());
        return;
    }
    PH_ASSERT(iter->second);
    scene().deleteEntity(iter->second);
    _lights.erase(iter);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::detachAllLights() {
    for (auto & l : _lights) scene().deleteEntity(l.second);
    _lights.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::detachAllComponents() {
    detachAllModels();
    detachAllLights();
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::setTransform(const Transform & transform) {
    // Ignore if no change.
    if (_local2Parent == transform) { return; }

    // Record the new value of the transform.
    _local2Parent = transform;

    // Record that the world transforms of this node and its children
    // will need to be updated before we use them again.
    setWorldTransformDirty();
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::setWorldTransform(const Transform & worldToParent) {
    // Fetch the parent of this node.
    Node * p = parent();

    // If this node has a parent.
    if (p != nullptr) {
        // Calculate the parent to local matrix.
        Transform parentToLocal = p->worldTransform().inverse();

        // Multiply the parent to local matrix against the transform to determine
        // what the transform is in the parent's space.
        Transform localTransform = parentToLocal * worldToParent;

        // Give the node the new local transform.
        setTransform(localTransform);

    } else {
        // If this node doesn't have a parent, we can just set its transform without calculation.
        setTransform(worldToParent);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::setVisible(bool v) {
    auto & s = scene();
    for (auto & m : _models) s.setVisible(m.second, v);
    for (auto & l : _lights) s.setVisible(l.second, v);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::setWorldTransformDirty() {
    // If this node is already dirty, nothing to do.
    // Even if a descendant became clean later on,
    // it would update this node to become clean too while cleaning itself,
    // so it wouldn't still be marked dirty.
    if (_worldTransformDirty) { return; }

    // set the whole subtree dirty
    bfsTraverseNodeGraph([](Node * n) -> TraverseAction {
        if (n->_worldTransformDirty)
            return TraverseAction::SKIP_SUBTREE;
        else {
            n->_worldTransformDirty = true;
            return TraverseAction::CONTINUE;
        }
    });
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::updateWorldTransform() const {
    // If world transform doesn't need to be updated.
    if (!_worldTransformDirty) {
        // Nothing to do.
        return;
    }

    // Add all successive dirty parents to the list of nodes to be updated.
    // Records the parent being processed.
    std::vector<const Node *> dirtyNodes;
    const Node *              p = this;
    while (p != nullptr) {
        // If this parent is dirty.
        if (p->_worldTransformDirty) {
            // Add parent to the list of nodes that need to be updated.
            PH_ASSERT(std::find(dirtyNodes.begin(), dirtyNodes.end(), p) == dirtyNodes.end());
            dirtyNodes.push_back(p);

            // Iterate to the next parent.
            PH_ASSERT(p->parent() != p);
            p = (Node *) p->parent();

            // If parent isn't dirty.
        } else {
            // We don't need to iterate any further.
            break;
        }
    }

    // Update the world transforms, starting from the topmost parent on down
    // so that their own results will be factored in to the calculation
    // of their children.
    while (dirtyNodes.size() > 0) {
        // Grab the top most dirty node.
        const Node * node = dirtyNodes.back();

        // Erase it from the stack.
        dirtyNodes.pop_back();

        // Update its world transform.
        node->recalculateWorldTransform();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Node::recalculateWorldTransform() const {
    // we should only call this when transform is actually dirty.
    PH_ASSERT(_worldTransformDirty);

    // Update this node's local to world transform.
    // Fetch the node's parent.
    const Node * p = (Node *) parent();

    // If this node has a parent.
    if (p != nullptr) {
        // Its local to world transform is the combination of its parent's
        // local to world transform and its own local to parent transform.
        _local2World = p->_local2World * _local2Parent;

        // If this node is a root node.
    } else {
        // Then its local to world transform is exactly the same as its local to parent's.
        _local2World = _local2Parent;
    }

    // Record that the node's world transform is now up to date.
    _worldTransformDirty = false;
}

// ---------------------------------------------------------------------------------------------------------------------
//
Graph::Graph(Scene & scene): _scene(scene) {
    // set name of root node
    _root.name = "root";
}

// ---------------------------------------------------------------------------------------------------------------------
//
Graph::~Graph() {
    auto p = &_root;
    deleteNodeAndSubtree(p);
}

// ---------------------------------------------------------------------------------------------------------------------
//
Node * Graph::createNode(Node * parent) {
    // validate parent node.
    if (parent && &parent->graph() != this) {
        PH_LOGE("can't create node with parent that belongs to different graph.");
        return nullptr;
    }
    // create new node
    auto n = new Node(*this, parent);
    if (!n) return nullptr;

    // new node is always in identity transform.
    n->setTransform({});

    // done
    n->_iter = _nodes.insert(_nodes.end(), n);
    return n;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Graph::deleteNodeAndSubtree(Node *& node) {
    if (!node) return;

    if (&node->graph() != this) {
        PH_LOGE("Can't delete node that belongs to different graph.");
        return;
    }

    // collect list of nodes that will be deleted.
    std::vector<Node *> toBeDeleted;
    node->bfsTraverseNodeGraph([&](Node * n) {
        toBeDeleted.push_back(n);
        return Node::TraverseAction::CONTINUE;
    });

    /// Now delete all nodes in the deletion list, in reversed order. Ignore the scene root node.
    for (auto iter = toBeDeleted.rbegin(); iter != toBeDeleted.rend(); ++iter) {
        Node * c = *iter;
        if (c != &_root) {
            _nodes.erase(c->_iter);
            delete c;
        }
    }

    // The target node is deleted, reset the parameter pointer to null.
    if (node != &_root) node = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Graph::refreshSceneGpuData(VkCommandBuffer cb) {
    for (auto n : _nodes) n->flushWorldTransform();
    scene().refreshGpuData(cb);
}

} // namespace sg