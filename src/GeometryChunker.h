#pragma once
// GeometryChunker.h - Phase 3
// Frustum Culling per-chunk + Small Feature Culling + LOD (3 levels)
//
// Moi chunk co cau truc:
//   ChunkGroup (CullCountCallback, SmallFeatureCulling)
//     └── osg::LOD
//           ├── [0   - nearDist]  HIGH:  box day du 12 tris
//           ├── [nearDist - farDist] MID: box don gian 4 tris (1 quad nhin tu tren)
//           └── [farDist - cullDist] LOW: point sprite / flat quad
//           (> cullDist: OSG tu cull)

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/LOD>
#include <osg/NodeCallback>
#include <osg/NodeVisitor>
#include <osg/LineWidth>
#include <osg/Depth>
#include <osg/BlendFunc>

#include "HardwareInstancing.h"

#include <vector>
#include <atomic>
#include <string>

// -------------------------------------------------------
// ChunkStats: dem chunk visible moi frame (atomic)
// -------------------------------------------------------
struct ChunkStats
{
    std::atomic<int> visibleChunks{ 0 };
    std::atomic<int> totalChunks  { 0 };
};

class CullCountCallback : public osg::NodeCallback
{
public:
    CullCountCallback(ChunkStats* s) : m_stats(s) {}
    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        m_stats->visibleChunks.fetch_add(1, std::memory_order_relaxed);
        traverse(node, nv);
    }
private:
    ChunkStats* m_stats;
};

// -------------------------------------------------------
// GeometryChunker
// -------------------------------------------------------
class GeometryChunker
{
public:
    // Khoang cach chuyen LOD (tinh theo don vi world units)
    struct LODConfig
    {
        // PIXEL_SIZE_ON_SCREEN mode (pixels tren man hinh)
        float smallPixels = 3.f;    // < 3px: cull (Small Feature Culling)
        // 3px → 150px : LOW  (xa, 1 quad)
        // 150px → 600px: MID  (5 mat)
        // > 600px       : HIGH (6 mat day du)
    };

    struct ChunkInfo
    {
        int chunkX, chunkZ;
        int instanceStart;
        int instanceCount;
        osg::BoundingBox bbox;
    };

    explicit GeometryChunker(const HardwareInstancing::Config& cfg,
                              const LODConfig& lodCfg = LODConfig());

    osg::ref_ptr<osg::Group> createScene(bool showWireframe = true);

    int         getTotalChunks()   const { return (int)m_chunks.size(); }
    int         getChunkDim()      const { return m_chunkDim; }
    int         getInstanceCount() const { return m_cfg.gridX * m_cfg.gridZ; }
    ChunkStats* getStats()               { return &m_stats; }

    // ---- Runtime toggles (khong can rebuild) ----

    // Frustum Culling: OSG test BBox voi camera frustum
    void setFrustumCulling(bool enabled);

    // LOD: doi qua HIGH-only khi tat
    void setLOD(bool enabled);

    // Small Feature Culling: an chunk < N pixels
    void setSmallFeatureCulling(bool enabled);

    bool getFrustumCulling()      const { return m_frustumCulling; }
    bool getLOD()                 const { return m_lodEnabled; }
    bool getSmallFeatureCulling() const { return m_smallFeature; }

private:
    // Tao geometry theo LOD level
    // level 0: HIGH - box day du (12 tris, 24 verts)
    // level 1: MID  - chi 5 mat (khong day, 10 tris, 20 verts)
    // level 2: LOW  - 1 flat quad nhin tu tren (2 tris, 4 verts)
    osg::ref_ptr<osg::Geometry> createBoxHigh(int n);
    osg::ref_ptr<osg::Geometry> createBoxMid (int n);
    osg::ref_ptr<osg::Geometry> createBoxLow (int n);

    osg::ref_ptr<osg::Program>  createShaderProgram();
    osg::ref_ptr<osg::Geode>    makeGeode(osg::ref_ptr<osg::Geometry> geom);
    osg::ref_ptr<osg::Geode>    createWireframeBBox(const osg::BoundingBox& bb);
    osg::ref_ptr<osg::Group>    buildChunk(const ChunkInfo& info,
                                           const std::vector<osg::Matrixf>& mats,
                                           const std::vector<osg::Vec4>& cols);
    void generateInstanceData();
    void reorderByChunk();

    HardwareInstancing::Config m_cfg;
    LODConfig                  m_lodCfg;
    int                        m_chunkDim;

    std::vector<osg::Matrixf>  m_matrices;
    std::vector<osg::Vec4>     m_colors;
    std::vector<ChunkInfo>     m_chunks;
    ChunkStats                 m_stats;

    osg::ref_ptr<osg::Group>   m_wireframeRoot;
    osg::ref_ptr<osg::Program> m_shaderProg;

    // Refs de toggle runtime
    std::vector<osg::ref_ptr<osg::Group>> m_chunkGroups; // frustum cull
    std::vector<osg::ref_ptr<osg::LOD>>   m_lodNodes;    // LOD + small feature

    // Current toggle states
    bool m_frustumCulling = true;
    bool m_lodEnabled     = true;
    bool m_smallFeature   = true;
};
