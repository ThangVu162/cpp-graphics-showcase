#pragma once
// HardwareInstancing.h
// Demo project - theo pattern TransformBufferCallback tu production code:
// - DrawCallback + raw GL cho TBO (transforms + colors)
// - #version 330 compatibility (gl_Vertex, gl_Normal built-ins)
// - Transforms TBO: 4 vec4 per instance (mat4, layout giong CHardwareInstancing)
// - Colors TBO: 1 vec4 per instance, dynamic update

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Drawable>
#include <osg/RenderInfo>
#include <osg/BoundingBox>
#include <osgUtil/SmoothingVisitor>
#include <vector>
#include <string>
#include <iostream>

// GLuint forward - glew.h chi duoc include trong .cpp (phai truoc gl.h)
typedef unsigned int GLuint;

// -------------------------------------------------------
// TransformBufferCallback
// Giong het class trong CHardwareInstancing production:
// - Upload mat4 transforms len TBO (tex unit 1)
// - Upload vec4 colors len TBO   (tex unit 2)
// - Dirty flag cho phep update colors runtime
// -------------------------------------------------------
struct TransformBufferCallback : public osg::Drawable::DrawCallback
{
    mutable GLuint tbo = 0, tboTex = 0;  // transforms
    mutable GLuint cbo = 0, cboTex = 0;  // colors
    mutable bool   initialized = false;

    int                       numInstances = 0;
    std::vector<osg::Matrixf> matrices;
    std::vector<osg::Vec4>* colors = nullptr;
    int* dirtyFlag = nullptr;

    TransformBufferCallback(
        const std::vector<osg::Matrixf>& mats,
        std::vector<osg::Vec4>*& colorVec,
        int*& dirty)
        : matrices(mats)
        , numInstances((int)mats.size())
        , colors(colorVec)
        , dirtyFlag(dirty)
    {
    }

    // Callback la owner cua colors va dirtyFlag
    virtual ~TransformBufferCallback()
    {
        delete colors;
        delete dirtyFlag;
    }

    void drawImplementation(
        osg::RenderInfo& renderInfo,
        const osg::Drawable* drawable) const override;
};

// -------------------------------------------------------
// InstancedBBoxCallback: bao OSG khong cull instanced geo
// -------------------------------------------------------
struct InstancedBBoxCallback : public osg::Drawable::ComputeBoundingBoxCallback
{
    osg::BoundingBox bbox;
    explicit InstancedBBoxCallback(const osg::BoundingBox& bb) : bbox(bb) {}
    osg::BoundingBox computeBound(const osg::Drawable&) const override { return bbox; }
};

// -------------------------------------------------------
// HardwareInstancing: tao demo scene voi N instanced boxes
// -------------------------------------------------------
class HardwareInstancing
{
public:
    struct Config
    {
        int          gridX = 100;
        int          gridZ = 100;
        float        spacing = 2.5f;
        float        minScale = 0.3f;
        float        maxScale = 1.2f;
        float        minHeight = 0.5f;
        float        maxHeight = 3.5f;
        unsigned int seed = 12345u;
    };

    explicit HardwareInstancing(const Config& cfg = Config());
    ~HardwareInstancing();

    osg::ref_ptr<osg::Group> createScene();

    int  getInstanceCount() const { return m_instanceCount; }
    void setInstanceColor(int index, const osg::Vec4& color);
    void resetInstanceColor(int index);

private:
    osg::ref_ptr<osg::Geometry> createBoxGeometry();
    void                        generateInstanceData();
    osg::ref_ptr<osg::Program>  createShaderProgram();
    osg::BoundingBox            computeUnionBBox(const osg::BoundingBox& localBB) const;

    Config                    m_cfg;
    int                       m_instanceCount;
    std::vector<osg::Matrixf> m_matrices;
    std::vector<osg::Vec4>    m_savedColors;
    std::vector<osg::Vec4>* m_colors = nullptr;
    int* m_dirty = nullptr;
};