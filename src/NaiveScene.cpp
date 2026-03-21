#include "NaiveScene.h"
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osg/Geometry>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <iostream>
#include <algorithm>

NaiveScene::NaiveScene(const HardwareInstancing::Config& cfg)
    : m_cfg(cfg)
    , m_frustumCulling(true)
    , m_lodEnabled(true)
    , m_smallFeature(true)
{}

osg::ref_ptr<osg::Group> NaiveScene::createScene()
{
    m_root = new osg::Group();
    m_geodes.clear();
    m_lodNodes.clear();

    srand(m_cfg.seed);
    auto randf = [](float lo, float hi) {
        return lo + (hi - lo) * (rand() / (float)RAND_MAX);
    };

    float totalW = (m_cfg.gridX - 1) * m_cfg.spacing;
    float totalD = (m_cfg.gridZ - 1) * m_cfg.spacing;

    for (int zi = 0; zi < m_cfg.gridZ; ++zi)
    for (int xi = 0; xi < m_cfg.gridX; ++xi)
    {
        float sx = randf(m_cfg.minScale,  m_cfg.maxScale);
        float sy = randf(m_cfg.minHeight, m_cfg.maxHeight);
        float sz = randf(m_cfg.minScale,  m_cfg.maxScale);
        float px = xi * m_cfg.spacing - totalW * 0.5f;
        float pz = zi * m_cfg.spacing - totalD * 0.5f;
        float py = sy * 0.5f;
        osg::Vec4 col(randf(.35f,1.f), randf(.35f,1.f), randf(.35f,1.f), 1.f);

        // HIGH: box day du
        auto* highSD = new osg::ShapeDrawable(
            new osg::Box(osg::Vec3(px,py,pz), sx, sy, sz));
        highSD->setColor(col);
        osg::ref_ptr<osg::Geode> highG = new osg::Geode();
        highG->addDrawable(highSD);

        // MID: box 80%
        auto* midSD = new osg::ShapeDrawable(
            new osg::Box(osg::Vec3(px,py,pz), sx*.8f, sy*.8f, sz*.8f));
        midSD->setColor(col);
        osg::ref_ptr<osg::Geode> midG = new osg::Geode();
        midG->addDrawable(midSD);

        // LOW: flat quad (2 tris) - nhe hon sphere
        osg::ref_ptr<osg::Geometry> lowGeom = new osg::Geometry();
        lowGeom->setUseVertexBufferObjects(true);
        osg::ref_ptr<osg::Vec3Array> lv = new osg::Vec3Array();
        float hs = std::max(sx, sz) * 0.5f;
        lv->push_back({px-hs, py, pz-hs});
        lv->push_back({px+hs, py, pz-hs});
        lv->push_back({px+hs, py, pz+hs});
        lv->push_back({px-hs, py, pz+hs});
        osg::ref_ptr<osg::Vec4Array> lc = new osg::Vec4Array();
        lc->push_back(col);
        lowGeom->setVertexArray(lv);
        lowGeom->setColorArray(lc, osg::Array::BIND_OVERALL);
        lowGeom->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        osg::ref_ptr<osg::Geode> lowG = new osg::Geode();
        lowG->addDrawable(lowGeom);

        // LOD PIXEL_SIZE_ON_SCREEN
        osg::ref_ptr<osg::LOD> lod = new osg::LOD();
        lod->setRangeMode(osg::LOD::PIXEL_SIZE_ON_SCREEN);
        lod->setCenterMode(osg::LOD::USE_BOUNDING_SPHERE_CENTER);
        lod->addChild(lowG,  3.f,   40.f);
        lod->addChild(midG,  40.f,  200.f);
        lod->addChild(highG, 200.f, 1e7f);
        m_lodNodes.push_back(lod);

        osg::ref_ptr<osg::Group> wrapper = new osg::Group();
        wrapper->addChild(lod);
        m_geodes.push_back(wrapper);
        m_root->addChild(wrapper);
    }

    std::cout << "[NaiveScene] " << m_cfg.gridX * m_cfg.gridZ
              << " objects | Frustum+LOD+SmallFeat ON\n";
    return m_root;
}

void NaiveScene::setFrustumCulling(bool on)
{
    m_frustumCulling = on;
    for (auto& g : m_geodes) g->setCullingActive(on);
    std::cout << "[NaiveScene] Frustum: " << (on?"ON":"OFF") << "\n";
}

void NaiveScene::setLOD(bool on)
{
    m_lodEnabled = on;
    for (auto& lod : m_lodNodes)
    {
        if (on) {
            float sp = m_smallFeature ? 3.f : 0.f;
            lod->setRange(0, sp,    40.f);
            lod->setRange(1, 40.f,  200.f);
            lod->setRange(2, 200.f, 1e7f);
        } else {
            lod->setRange(0, 1e7f, 1e7f);
            lod->setRange(1, 1e7f, 1e7f);
            lod->setRange(2, 0.f,  1e7f);
        }
    }
    std::cout << "[NaiveScene] LOD: " << (on?"ON":"OFF") << "\n";
}

void NaiveScene::setSmallFeatureCulling(bool on)
{
    m_smallFeature = on;
    if (!m_lodEnabled) return;
    float sp = on ? 3.f : 0.f;
    for (auto& lod : m_lodNodes) {
        float curMax = lod->getMaxRange(0);
        lod->setRange(0, sp, curMax);
    }
    std::cout << "[NaiveScene] SmallFeat: " << (on?"ON":"OFF") << "\n";
}
