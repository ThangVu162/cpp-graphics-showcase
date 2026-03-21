#include <GL/glew.h>
#include "GeometryChunker.h"

#include <osg/BlendFunc>
#include <osg/LineWidth>
#include <osg/Depth>

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>

// -------------------------------------------------------
// Shared shader (giong HardwareInstancing)
// -------------------------------------------------------
static const std::string s_vert = R"(
#version 330 compatibility
uniform samplerBuffer transforms;
uniform samplerBuffer instanceColors;
out vec4 vColor;
void main()
{
    int idx = gl_InstanceID * 4;
    mat4 m = mat4(
        texelFetch(transforms, idx+0),
        texelFetch(transforms, idx+1),
        texelFetch(transforms, idx+2),
        texelFetch(transforms, idx+3)
    );
    vColor      = texelFetch(instanceColors, gl_InstanceID);
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * m * gl_Vertex;
}
)";

static const std::string s_frag = R"(
#version 330 compatibility
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
)";

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
GeometryChunker::GeometryChunker(const HardwareInstancing::Config& cfg,
                                  const LODConfig& lodCfg)
    : m_cfg(cfg), m_lodCfg(lodCfg)
{
    m_chunkDim = std::max(1, (int)std::ceil(std::sqrt((double)cfg.gridX)));

    int nCX = (cfg.gridX + m_chunkDim - 1) / m_chunkDim;
    int nCZ = (cfg.gridZ + m_chunkDim - 1) / m_chunkDim;
    m_stats.totalChunks.store(nCX * nCZ);

    std::cout << "[Chunker] Grid " << cfg.gridX << "x" << cfg.gridZ
              << " | ChunkDim=" << m_chunkDim
              << " | Chunks=" << nCX << "x" << nCZ << "=" << nCX*nCZ
              << " | LOD pixel mode cull<" << lodCfg.smallPixels << "px\n";
}

// -------------------------------------------------------
// Shader program (shared)
// -------------------------------------------------------
osg::ref_ptr<osg::Program> GeometryChunker::createShaderProgram()
{
    osg::ref_ptr<osg::Program> p = new osg::Program();
    p->setName("ChunkLOD");
    p->addShader(new osg::Shader(osg::Shader::VERTEX,   s_vert));
    p->addShader(new osg::Shader(osg::Shader::FRAGMENT, s_frag));
    return p;
}

// -------------------------------------------------------
// Helper: tao Geode tu Geometry + setup shader state
// -------------------------------------------------------
osg::ref_ptr<osg::Geode> GeometryChunker::makeGeode(
    osg::ref_ptr<osg::Geometry> geom)
{
    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setAttribute(m_shaderProg, osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING,
        osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    osg::ref_ptr<osg::Geode> g = new osg::Geode();
    g->addDrawable(geom);
    return g;
}

// -------------------------------------------------------
// LOD Level 0: HIGH — box day du 6 mat, 12 tris
// -------------------------------------------------------
osg::ref_ptr<osg::Geometry> GeometryChunker::createBoxHigh(int n)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseVertexBufferObjects(true);
    geom->setUseDisplayList(false);

    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array();
    struct Face { osg::Vec3 pts[4]; };
    static const Face faces[6] = {
        { { {.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f},{.5f,-.5f,.5f} } },
        { { {-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,-.5f} } },
        { { {-.5f,.5f,-.5f},{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f} } },
        { { {-.5f,-.5f,.5f},{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f} } },
        { { {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f} } },
        { { {.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f} } },
    };
    for (const auto& f : faces)
        for (int i = 0; i < 4; ++i) v->push_back(f.pts[i]);

    osg::ref_ptr<osg::DrawElementsUShort> idx =
        new osg::DrawElementsUShort(GL_TRIANGLES);
    for (int f = 0; f < 6; ++f) {
        int b = f*4;
        idx->push_back(b); idx->push_back(b+1); idx->push_back(b+2);
        idx->push_back(b); idx->push_back(b+2); idx->push_back(b+3);
    }
    idx->setNumInstances(n);
    geom->setVertexArray(v);
    geom->addPrimitiveSet(idx);
    return geom;
}

// -------------------------------------------------------
// LOD Level 1: MID — chi 5 mat (bo day duoi), 10 tris
// Giam 17% vertex/tri, nhat la khi nhieu box chong len nhau
// -------------------------------------------------------
osg::ref_ptr<osg::Geometry> GeometryChunker::createBoxMid(int n)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseVertexBufferObjects(true);
    geom->setUseDisplayList(false);

    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array();
    // +X, -X, +Y(top), +Z, -Z  — bo mat -Y (day)
    struct Face { osg::Vec3 pts[4]; };
    static const Face faces[5] = {
        { { {.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f},{.5f,-.5f,.5f} } },
        { { {-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,-.5f} } },
        { { {-.5f,.5f,-.5f},{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f} } },  // top
        { { {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f} } },
        { { {.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f} } },
    };
    for (const auto& f : faces)
        for (int i = 0; i < 4; ++i) v->push_back(f.pts[i]);

    osg::ref_ptr<osg::DrawElementsUShort> idx =
        new osg::DrawElementsUShort(GL_TRIANGLES);
    for (int f = 0; f < 5; ++f) {
        int b = f*4;
        idx->push_back(b); idx->push_back(b+1); idx->push_back(b+2);
        idx->push_back(b); idx->push_back(b+2); idx->push_back(b+3);
    }
    idx->setNumInstances(n);
    geom->setVertexArray(v);
    geom->addPrimitiveSet(idx);
    return geom;
}

// -------------------------------------------------------
// LOD Level 2: LOW — 1 flat quad nhin tu tren (2 tris)
// Chi hien thi mat tren, rat nhanh khi camera o xa
// -------------------------------------------------------
osg::ref_ptr<osg::Geometry> GeometryChunker::createBoxLow(int n)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseVertexBufferObjects(true);
    geom->setUseDisplayList(false);

    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array();
    // Chi mat tren (+Y)
    v->push_back({-.5f,.5f,-.5f});
    v->push_back({-.5f,.5f, .5f});
    v->push_back({ .5f,.5f, .5f});
    v->push_back({ .5f,.5f,-.5f});

    osg::ref_ptr<osg::DrawElementsUShort> idx =
        new osg::DrawElementsUShort(GL_TRIANGLES);
    idx->push_back(0); idx->push_back(1); idx->push_back(2);
    idx->push_back(0); idx->push_back(2); idx->push_back(3);
    idx->setNumInstances(n);

    geom->setVertexArray(v);
    geom->addPrimitiveSet(idx);
    return geom;
}

// -------------------------------------------------------
// Wireframe BBox
// -------------------------------------------------------
osg::ref_ptr<osg::Geode> GeometryChunker::createWireframeBBox(
    const osg::BoundingBox& bb)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array();

    float x0=bb.xMin(),x1=bb.xMax(),y0=bb.yMin(),y1=bb.yMax(),z0=bb.zMin(),z1=bb.zMax();
    // 12 edges = 24 verts (GL_LINES)
    osg::Vec3 corners[8] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
        {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}
    };
    int edges[24] = {0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7};
    for (int i : edges) v->push_back(corners[i]);

    osg::ref_ptr<osg::Vec4Array> col = new osg::Vec4Array();
    col->push_back({1,1,0,.5f});
    geom->setVertexArray(v);
    geom->setColorArray(col, osg::Array::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 24));

    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setAttribute(new osg::LineWidth(1.2f));
    ss->setMode(GL_BLEND, osg::StateAttribute::ON);
    ss->setAttribute(new osg::BlendFunc(
        osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA));
    osg::ref_ptr<osg::Depth> d = new osg::Depth();
    d->setWriteMask(false);
    ss->setAttribute(d);
    ss->setRenderBinDetails(10, "DepthSortedBin");

    osg::ref_ptr<osg::Geode> g = new osg::Geode();
    g->addDrawable(geom);
    return g;
}

// -------------------------------------------------------
// Generate flat instance data (seed-based random)
// -------------------------------------------------------
void GeometryChunker::generateInstanceData()
{
    int total = m_cfg.gridX * m_cfg.gridZ;
    m_matrices.resize(total);
    m_colors.resize(total);

    srand(m_cfg.seed);
    auto randf = [](float lo, float hi) {
        return lo + (hi-lo)*(rand()/(float)RAND_MAX);
    };
    float totalW = (m_cfg.gridX-1)*m_cfg.spacing;
    float totalD = (m_cfg.gridZ-1)*m_cfg.spacing;

    for (int zi = 0; zi < m_cfg.gridZ; ++zi)
    for (int xi = 0; xi < m_cfg.gridX; ++xi)
    {
        int i = zi*m_cfg.gridX + xi;
        float sx = randf(m_cfg.minScale,  m_cfg.maxScale);
        float sy = randf(m_cfg.minHeight, m_cfg.maxHeight);
        float sz = randf(m_cfg.minScale,  m_cfg.maxScale);
        float px = xi*m_cfg.spacing - totalW*0.5f;
        float pz = zi*m_cfg.spacing - totalD*0.5f;
        float py = sy*0.5f;

        m_matrices[i] = osg::Matrixf(
            sx,0,0,0,  0,sy,0,0,  0,0,sz,0,  px,py,pz,1);
        m_colors[i] = osg::Vec4(
            randf(.35f,1.f), randf(.35f,1.f), randf(.35f,1.f), 1.f);
    }
}

// -------------------------------------------------------
// Build 1 chunk voi LOD + SmallFeatureCulling
// -------------------------------------------------------
osg::ref_ptr<osg::Group> GeometryChunker::buildChunk(
    const ChunkInfo& info,
    const std::vector<osg::Matrixf>& allMats,
    const std::vector<osg::Vec4>&    allCols)
{
    int n = info.instanceCount;
    if (n <= 0) return nullptr;

    // Extract subset nay tu flat array (da reorder roi)
    std::vector<osg::Matrixf> mats(allMats.begin() + info.instanceStart,
                                    allMats.begin() + info.instanceStart + n);
    std::vector<osg::Vec4>*   cols = new std::vector<osg::Vec4>(
        allCols.begin() + info.instanceStart,
        allCols.begin() + info.instanceStart + n);

    // Tao TBO callback (shared giua 3 LOD levels)
    // Moi LOD level dung cung TBO nhung geometry khac nhau
    int* dirty = new int(0);
    osg::ref_ptr<TransformBufferCallback> cb =
        new TransformBufferCallback(mats, cols, dirty);

    // --- 3 LOD Geometries ---
    auto makeGeodeLOD = [&](osg::ref_ptr<osg::Geometry> geom)
        -> osg::ref_ptr<osg::Geode>
    {
        geom->setDrawCallback(cb);   // cung 1 TBO callback
        geom->setComputeBoundingBoxCallback(new InstancedBBoxCallback(info.bbox));
        geom->dirtyBound();
        return makeGeode(geom);
    };

    osg::ref_ptr<osg::Geode> highGeode = makeGeodeLOD(createBoxHigh(n));
    osg::ref_ptr<osg::Geode> midGeode  = makeGeodeLOD(createBoxMid(n));
    osg::ref_ptr<osg::Geode> lowGeode  = makeGeodeLOD(createBoxLow(n));

    // --- LOD Node dung PIXEL_SIZE_ON_SCREEN ---
    // Range = kich thuoc chunk tren man hinh (pixels)
    // Lon hon = gan camera hon = chi tiet cao hon
    // Ket hop luon Small Feature Culling: < smallPixels thi khong co child → cull
    osg::ref_ptr<osg::LOD> lod = new osg::LOD();
    lod->setCenterMode(osg::LOD::USE_BOUNDING_SPHERE_CENTER);
    lod->setRangeMode(osg::LOD::PIXEL_SIZE_ON_SCREEN);

    // Pixel size tang dan = gan camera dan
    // [smallPixels → 150px] : LOW   (xa, it pixel)
    // [150px → 600px]       : MID
    // [600px → max]         : HIGH  (gan, nhieu pixel)
    float sp = m_lodCfg.smallPixels;
    lod->addChild(lowGeode,  sp,    150.f);
    lod->addChild(midGeode,  150.f, 600.f);
    lod->addChild(highGeode, 600.f, 1e7f);
    // < sp pixels: khong co child → OSG tu cull (Small Feature Culling!)

    // --- Chunk Group ---
    osg::ref_ptr<osg::Group> chunkGroup = new osg::Group();
    chunkGroup->setName("Chunk_" + std::to_string(info.chunkX)
                        + "_" + std::to_string(info.chunkZ));
    chunkGroup->setInitialBound(info.bbox);
    chunkGroup->addCullCallback(new CullCountCallback(&m_stats));

    chunkGroup->addChild(lod);

    // Luu refs de toggle runtime
    m_chunkGroups.push_back(chunkGroup);
    m_lodNodes.push_back(lod);

    return chunkGroup;
}

// -------------------------------------------------------
// Toggle: Frustum Culling
// setCullingActive(false) → OSG bo qua BBox test, luon traverse
// -------------------------------------------------------
void GeometryChunker::setFrustumCulling(bool enabled)
{
    m_frustumCulling = enabled;
    for (auto& g : m_chunkGroups)
        g->setCullingActive(enabled);
    std::cout << "[Chunker] Frustum Culling: "
              << (enabled ? "ON" : "OFF") << "\n";
}

// -------------------------------------------------------
// Toggle: LOD
// OFF → tat ca chunks dung HIGH detail (range [0, 1e7])
// ON  → khoi phuc 3 levels voi pixel ranges
// -------------------------------------------------------
void GeometryChunker::setLOD(bool enabled)
{
    m_lodEnabled = enabled;
    float sp = m_lodCfg.smallPixels;

    for (auto& lod : m_lodNodes)
    {
        if (enabled)
        {
            // Khoi phuc 3-level pixel ranges
            // child 0 = LOW, 1 = MID, 2 = HIGH
            lod->setRange(0, sp,    150.f);
            lod->setRange(1, 150.f, 600.f);
            lod->setRange(2, 600.f, 1e7f);
        }
        else
        {
            // Chi show HIGH (child 2), range [0, 1e7]
            lod->setRange(0, 1e7f, 1e7f);  // LOW: khong bao gio show
            lod->setRange(1, 1e7f, 1e7f);  // MID: khong bao gio show
            lod->setRange(2, 0.f,  1e7f);  // HIGH: luon show
        }
    }
    std::cout << "[Chunker] LOD: " << (enabled ? "ON" : "OFF") << "\n";
}

// -------------------------------------------------------
// Toggle: Small Feature Culling
// OFF → ha min range cua LOW xuong 0 (khong cull nua)
// ON  → khoi phuc min range = smallPixels
// -------------------------------------------------------
void GeometryChunker::setSmallFeatureCulling(bool enabled)
{
    m_smallFeature = enabled;
    float sp = enabled ? m_lodCfg.smallPixels : 0.f;

    for (auto& lod : m_lodNodes)
    {
        if (m_lodEnabled)
        {
            // Chi thay doi min range cua LOW (child 0)
            // max range giu nguyen de khong anh huong LOD levels
            float curMax = lod->getMaxRange(0);
            lod->setRange(0, sp, curMax);
        }
    }
    std::cout << "[Chunker] Small Feature Culling: "
              << (enabled ? "ON (cull < " + std::to_string((int)m_lodCfg.smallPixels) + "px)" : "OFF") << "\n";
}


osg::ref_ptr<osg::Group> GeometryChunker::createScene(bool showWireframe)
{
    generateInstanceData();
    m_shaderProg = createShaderProgram();
    m_chunkGroups.clear();
    m_lodNodes.clear();

    int nCX = (m_cfg.gridX + m_chunkDim - 1) / m_chunkDim;
    int nCZ = (m_cfg.gridZ + m_chunkDim - 1) / m_chunkDim;

    // --- Build chunk infos + compute BBoxes ---
    m_chunks.clear();
    m_chunks.reserve(nCX * nCZ);

    // Reorder m_matrices/m_colors theo chunk order
    std::vector<osg::Matrixf> reordMats;
    std::vector<osg::Vec4>    reordCols;
    reordMats.reserve(m_matrices.size());
    reordCols.reserve(m_colors.size());

    for (int czIdx = 0; czIdx < nCZ; ++czIdx)
    for (int cxIdx = 0; cxIdx < nCX; ++cxIdx)
    {
        int xiS = cxIdx * m_chunkDim,  xiE = std::min(xiS + m_chunkDim, m_cfg.gridX);
        int ziS = czIdx * m_chunkDim,  ziE = std::min(ziS + m_chunkDim, m_cfg.gridZ);

        ChunkInfo info;
        info.chunkX        = cxIdx;
        info.chunkZ        = czIdx;
        info.instanceStart = (int)reordMats.size();
        info.instanceCount = 0;

        for (int zi = ziS; zi < ziE; ++zi)
        for (int xi = xiS; xi < xiE; ++xi)
        {
            int src = zi * m_cfg.gridX + xi;
            reordMats.push_back(m_matrices[src]);
            reordCols.push_back(m_colors [src]);

            // Expand BBox tu transform matrix
            osg::Vec3 c(m_matrices[src](3,0),
                         m_matrices[src](3,1),
                         m_matrices[src](3,2));
            osg::Vec3 h(std::abs(m_matrices[src](0,0))*0.5f + 0.01f,
                         std::abs(m_matrices[src](1,1))*0.5f + 0.01f,
                         std::abs(m_matrices[src](2,2))*0.5f + 0.01f);
            info.bbox.expandBy(c - h);
            info.bbox.expandBy(c + h);
            ++info.instanceCount;
        }

        m_chunks.push_back(info);
    }

    m_matrices = reordMats;
    m_colors   = reordCols;

    // --- Build scene ---
    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->setName("ChunkerRoot");
    m_wireframeRoot = new osg::Group();
    m_wireframeRoot->setName("WireframeRoot");

    for (const auto& info : m_chunks)
    {
        osg::ref_ptr<osg::Group> cg = buildChunk(info, m_matrices, m_colors);
        if (cg.valid())
        {
            root->addChild(cg);
            if (showWireframe)
                m_wireframeRoot->addChild(createWireframeBBox(info.bbox));
        }
    }

    if (showWireframe && m_wireframeRoot->getNumChildren() > 0)
        root->addChild(m_wireframeRoot);

    std::cout << "[Chunker] Built " << m_chunks.size() << " chunks"
              << " | LOD PIXEL_SIZE mode:"
              << " LOW=" << m_lodCfg.smallPixels << "-150px"
              << " MID=150-600px HIGH>600px"
              << " cull<" << m_lodCfg.smallPixels << "px\n";
    return root;
}
