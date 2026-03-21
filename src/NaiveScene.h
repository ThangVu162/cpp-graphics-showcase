#pragma once
#include <osg/Group>
#include <osg/Geode>
#include <osg/LOD>
#include "HardwareInstancing.h"
#include <vector>

class NaiveScene
{
    HardwareInstancing::Config            m_cfg;
    osg::ref_ptr<osg::Group>              m_root;
    std::vector<osg::ref_ptr<osg::Group>> m_geodes;
    std::vector<osg::ref_ptr<osg::LOD>>   m_lodNodes;
    bool m_frustumCulling = true;
    bool m_lodEnabled     = true;
    bool m_smallFeature   = true;

public:
    explicit NaiveScene(const HardwareInstancing::Config& cfg);

    osg::ref_ptr<osg::Group> createScene();

    void setFrustumCulling(bool on);
    void setLOD(bool on);
    void setSmallFeatureCulling(bool on);

    bool getFrustumCulling()      const { return m_frustumCulling; }
    bool getLOD()                 const { return m_lodEnabled; }
    bool getSmallFeatureCulling() const { return m_smallFeature; }
};
