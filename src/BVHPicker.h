#pragma once
// BVHPicker.h
// Ray picking cho instanced geometry:
// 1. Pre-build BVH tu AABB cua tung instance
// 2. Ray-BVH → candidates O(log N)
// 3. Di chuyen proxy geometry den vi tri candidate
// 4. LineSegmentIntersector tren proxy → hit chinh xac
// 5. Sort by distance → tra ve instance index gan nhat

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/BoundingBox>
#include <osg/LineWidth>
#include <osgUtil/LineSegmentIntersector>
#include <osgUtil/IntersectionVisitor>

#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <limits>
#include <iostream>
#include <osg/Depth>

// -------------------------------------------------------
// BVH Node (binary tree, AABB-based)
// -------------------------------------------------------
struct BVHNode
{
    osg::BoundingBox aabb;
    int left  = -1;   // index trong m_nodes, -1 = khong co
    int right = -1;
    int instanceIdx = -1; // >= 0: la leaf node
};

// -------------------------------------------------------
// BVHPicker
// -------------------------------------------------------
class BVHPicker
{
public:
    struct InstanceData
    {
        osg::Matrixd     matrix;    // world transform cua instance
        osg::BoundingBox worldAABB; // pre-computed world AABB
        int              index;     // index trong instance array
    };

    struct PickResult
    {
        int    instanceIndex = -1; // -1 = khong hit
        double distance      = std::numeric_limits<double>::max();
        osg::Vec3d hitPoint;
    };

    BVHPicker() {}

    // Dang ky tap instances
    // localBB: bounding box cua 1 unit box trong local space
    void build(const std::vector<osg::Matrixf>& matrices,
               const osg::BoundingBox& localBB)
    {
        m_instances.clear();
        m_nodes.clear();

        int n = (int)matrices.size();
        m_instances.reserve(n);

        for (int i = 0; i < n; ++i)
        {
            osg::Matrixd mat(matrices[i]); // float → double
            osg::BoundingBox worldBB;

            // Transform 8 corners cua localBB sang world space
            for (int c = 0; c < 8; ++c)
            {
                osg::Vec3 corner(
                    (c & 1) ? localBB.xMax() : localBB.xMin(),
                    (c & 2) ? localBB.yMax() : localBB.yMin(),
                    (c & 4) ? localBB.zMax() : localBB.zMin());
                worldBB.expandBy(mat.preMult(corner));
            }

            m_instances.push_back({mat, worldBB, i});
        }

        // Build BVH
        m_nodes.reserve(n * 2);
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        _buildRecursive(indices, 0, n);

        std::cout << "[BVHPicker] Built BVH: " << n << " instances, "
                  << m_nodes.size() << " nodes\n";
    }

    // Set proxy geometry (shared voi instanced scene)
    void setProxy(osg::ref_ptr<osg::Geometry> proxyGeom,
                  osg::ref_ptr<osg::Group>    proxyRoot)
    {
        m_proxyGeom = proxyGeom;

        m_proxyXform = new osg::MatrixTransform();
        osg::ref_ptr<osg::Geode> pg = new osg::Geode();
        pg->addDrawable(proxyGeom);
        m_proxyXform->addChild(pg);
        m_proxyXform->setNodeMask(0);
        proxyRoot->addChild(m_proxyXform);

        // --- Highlight node: wireframe box xung quanh instance duoc pick ---
        m_highlightXform = new osg::MatrixTransform();
        m_highlightXform->addChild(_createHighlightBox());
        m_highlightXform->setNodeMask(0); // an cho den khi co pick
        proxyRoot->addChild(m_highlightXform);

        m_proxyRoot = proxyRoot;
    }

    // Hien/an highlight box tai vi tri instance
    void showHighlight(int instanceIndex)
    {
        if (instanceIndex < 0 || instanceIndex >= (int)m_instances.size())
        {
            m_highlightXform->setNodeMask(0);
            return;
        }
        m_highlightXform->setMatrix(m_instances[instanceIndex].matrix);
        m_highlightXform->setNodeMask(~0u);
    }

    void hideHighlight()
    {
        if (m_highlightXform.valid())
            m_highlightXform->setNodeMask(0);
    }

    // Pick: tra ve instance index gan nhat, -1 neu khong hit
    PickResult pick(const osg::Vec3d& rayOrig,
                    const osg::Vec3d& rayDir)
    {
        if (m_nodes.empty()) return {};

        // --- Buoc 1: BVH broad phase → candidates ---
        std::vector<std::pair<double, int>> candidates; // (tMin, instanceIdx)
        _queryCandidates(0, rayOrig, rayDir, candidates);

        if (candidates.empty()) return {};

        // Sort gan → xa
        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b){ return a.first < b.first; });

        PickResult best;

        // --- Buoc 2: Proxy narrow phase ---
        for (int ci = 0; ci < (int)candidates.size(); ++ci)
        {
            double tMin    = candidates[ci].first;
            int    instIdx = candidates[ci].second;

            if (tMin > best.distance) break; // da tim thay hit gan hon

            const InstanceData& inst = m_instances[instIdx];

            // Di chuyen proxy den vi tri instance
            m_proxyXform->setMatrix(inst.matrix);
            m_proxyXform->setNodeMask(~0u); // hien proxy

            // LineSegmentIntersector
            osg::Vec3d rayEnd = rayOrig + rayDir * 1e7;
            osg::ref_ptr<osgUtil::LineSegmentIntersector> lsi =
                new osgUtil::LineSegmentIntersector(rayOrig, rayEnd);
            osgUtil::IntersectionVisitor iv(lsi);
            m_proxyXform->accept(iv);

            m_proxyXform->setNodeMask(0); // an proxy

            if (lsi->containsIntersections())
            {
                const auto& hit = *lsi->getIntersections().begin();
                osg::Vec3d hitPt = hit.getWorldIntersectPoint();
                double dist = (hitPt - rayOrig).length();

                if (dist < best.distance)
                {
                    best.instanceIndex = instIdx;
                    best.distance      = dist;
                    best.hitPoint      = hitPt;
                }
            }
        }

        return best;
    }

    int getInstanceCount() const { return (int)m_instances.size(); }

private:
    // -------------------------------------------------------
    // BVH build: chia theo axis dai nhat, nth_element
    // -------------------------------------------------------
    int _buildRecursive(std::vector<int>& idxs, int start, int end)
    {
        BVHNode node;
        for (int i = start; i < end; ++i)
            node.aabb.expandBy(m_instances[idxs[i]].worldAABB);

        int nodeIdx = (int)m_nodes.size();
        m_nodes.push_back(node);

        int count = end - start;
        if (count == 1)
        {
            m_nodes[nodeIdx].instanceIdx = idxs[start];
            return nodeIdx;
        }

        // Axis dai nhat
        osg::Vec3 sz = node.aabb._max - node.aabb._min;
        int axis = (sz.y() > sz.x()) ? 1 : 0;
        if (sz.z() > sz[axis]) axis = 2;

        int mid = start + count / 2;
        std::nth_element(idxs.begin() + start,
                         idxs.begin() + mid,
                         idxs.begin() + end,
            [&](int a, int b) {
                return m_instances[a].worldAABB.center()[axis]
                     < m_instances[b].worldAABB.center()[axis];
            });

        m_nodes[nodeIdx].left  = _buildRecursive(idxs, start, mid);
        m_nodes[nodeIdx].right = _buildRecursive(idxs, mid,   end);
        return nodeIdx;
    }

    // -------------------------------------------------------
    // Ray vs AABB slab test
    // -------------------------------------------------------
    bool _rayAABB(const osg::Vec3d& orig, const osg::Vec3d& dir,
                  const osg::BoundingBox& bb, double& tMin) const
    {
        double tMax = 1e18;
        tMin = -1e18;
        for (int i = 0; i < 3; ++i)
        {
            double invD = (std::abs(dir[i]) < 1e-12)
                        ? (dir[i] >= 0 ? 1e12 : -1e12)
                        : 1.0 / dir[i];
            double t0 = (bb._min[i] - orig[i]) * invD;
            double t1 = (bb._max[i] - orig[i]) * invD;
            if (invD < 0) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMax < tMin) return false;
        }
        return tMax >= 0;
    }

    // -------------------------------------------------------
    // Traverse BVH, collect candidates
    // -------------------------------------------------------
    void _queryCandidates(int nodeIdx,
                          const osg::Vec3d& orig,
                          const osg::Vec3d& dir,
                          std::vector<std::pair<double,int>>& out) const
    {
        if (nodeIdx < 0 || nodeIdx >= (int)m_nodes.size()) return;
        const BVHNode& node = m_nodes[nodeIdx];

        double t;
        if (!_rayAABB(orig, dir, node.aabb, t)) return;

        if (node.instanceIdx >= 0) // leaf
        {
            out.push_back({t, node.instanceIdx});
            return;
        }
        _queryCandidates(node.left,  orig, dir, out);
        _queryCandidates(node.right, orig, dir, out);
    }

    std::vector<InstanceData>          m_instances;
    std::vector<BVHNode>               m_nodes;

    osg::ref_ptr<osg::Geometry>        m_proxyGeom;
    osg::ref_ptr<osg::MatrixTransform> m_proxyXform;
    osg::ref_ptr<osg::MatrixTransform> m_highlightXform; // wireframe box
    osg::ref_ptr<osg::Group>           m_proxyRoot;

    // Tao wireframe box highlight (scale 1.1x de bao quanh instance)
    osg::ref_ptr<osg::Group> _createHighlightBox()
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array();

        // 8 corners cua unit box, scale 1.1 de hien thi ra ngoai
        float s = 0.55f; // 0.5 * 1.1
        osg::Vec3 c[8] = {
            {-s,-s,-s},{s,-s,-s},{s,s,-s},{-s,s,-s},
            {-s,-s, s},{s,-s, s},{s,s, s},{-s,s, s}
        };
        // 12 canh
        int edges[24] = {0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7};
        for (int i : edges) v->push_back(c[i]);

        // Mau vang neon
        osg::ref_ptr<osg::Vec4Array> col = new osg::Vec4Array();
        col->push_back({1.f, 0.95f, 0.f, 1.f});

        geom->setVertexArray(v);
        geom->setColorArray(col, osg::Array::BIND_OVERALL);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 24));

        // StateSet: luon hien thi qua geometry, khong bi depth-cull
        osg::StateSet* ss = geom->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        osg::ref_ptr<osg::LineWidth> lw = new osg::LineWidth(2.5f);
        ss->setAttribute(lw);

        // Depth test LEQUAL de hien thi vien ngoai, khong bi che boi chinh no
        osg::ref_ptr<osg::Depth> depth = new osg::Depth(osg::Depth::LEQUAL);
        ss->setAttribute(depth);

        ss->setRenderBinDetails(999, "RenderBin"); // render sau cung

        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        geode->addDrawable(geom);

        osg::ref_ptr<osg::Group> g = new osg::Group();
        g->addChild(geode);
        return g;
    }
};
