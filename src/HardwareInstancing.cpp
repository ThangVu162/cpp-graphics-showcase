// glew.h PHAI duoc include truoc bat ky OpenGL header nao (ke ca OSG)
#include <GL/glew.h>

#include "HardwareInstancing.h"
#include <osg/MatrixTransform>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// -------------------------------------------------------
// GLSL Shaders - giong pattern production nhung don gian hon
// (khong co texture, material; chi co transform + color + lighting)
// -------------------------------------------------------
static const std::string s_vertSrc = R"(
#version 330 compatibility

uniform samplerBuffer transforms;
uniform samplerBuffer instanceColors;

out vec4 vColor;

void main()
{
    int idx = gl_InstanceID * 4;
    mat4 instMat = mat4(
        texelFetch(transforms, idx + 0),
        texelFetch(transforms, idx + 1),
        texelFetch(transforms, idx + 2),
        texelFetch(transforms, idx + 3)
    );

    vColor      = texelFetch(instanceColors, gl_InstanceID);
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * instMat * gl_Vertex;
}
)";

static const std::string s_fragSrc = R"(
#version 330 compatibility

in vec4 vColor;

out vec4 fragColor;

void main()
{
    fragColor = vColor;
}
)";

// -------------------------------------------------------
// TransformBufferCallback destructor - cleanup GL resources
// -------------------------------------------------------
TransformBufferCallback::~TransformBufferCallback()
{
    if (initialized)
    {
        if (tbo)    glDeleteBuffers(1, &tbo);
        if (tboTex) glDeleteTextures(1, &tboTex);
        if (cbo)    glDeleteBuffers(1, &cbo);
        if (cboTex) glDeleteTextures(1, &cboTex);
    }
    delete colors;
    delete dirtyFlag;
}

// -------------------------------------------------------
// TransformBufferCallback::drawImplementation
// Giong het production: init GLEW, tao TBO, bind, draw
// -------------------------------------------------------
void TransformBufferCallback::drawImplementation(
    osg::RenderInfo& renderInfo,
    const osg::Drawable* drawable) const
{
    if (!initialized)
    {
        glewExperimental = GL_TRUE; // force load tat ca function pointers
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            std::cerr << "[TBO] GLEW init failed: "
                      << glewGetErrorString(err) << "\n";
            return;
        }
        glGetError(); // clear GL_INVALID_ENUM do glewInit co the sinh ra

        // --- Transforms TBO (STATIC) ---
        glGenBuffers(1, &tbo);
        glBindBuffer(GL_TEXTURE_BUFFER, tbo);
        glBufferData(GL_TEXTURE_BUFFER,
            sizeof(osg::Matrixf) * numInstances,
            matrices.data(), GL_STATIC_DRAW);
        glGenTextures(1, &tboTex);
        glBindTexture(GL_TEXTURE_BUFFER, tboTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, tbo);

        // --- Colors TBO (DYNAMIC) ---
        glGenBuffers(1, &cbo);
        glBindBuffer(GL_TEXTURE_BUFFER, cbo);
        glBufferData(GL_TEXTURE_BUFFER,
            sizeof(osg::Vec4) * numInstances,
            colors->data(), GL_DYNAMIC_DRAW);
        glGenTextures(1, &cboTex);
        glBindTexture(GL_TEXTURE_BUFFER, cboTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, cbo);

        initialized = true;
        *dirtyFlag  = 0;
        std::cout << "[TBO] Initialized " << numInstances << " instances\n";
    }

    // Re-upload colors neu dirty (highlight, hover, reset)
    if (*dirtyFlag)
    {
        glBindBuffer(GL_TEXTURE_BUFFER, cbo);
        glBufferData(GL_TEXTURE_BUFFER,
            sizeof(osg::Vec4) * numInstances,
            nullptr, GL_DYNAMIC_DRAW); // orphan
        glBufferSubData(GL_TEXTURE_BUFFER, 0,
            sizeof(osg::Vec4) * numInstances,
            colors->data());
        *dirtyFlag = 0;
    }

    // Bind TBOs vao tex unit 1 (transforms) va 2 (colors)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, tboTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_BUFFER, cboTex);

    // Set sampler uniforms
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    GLint loc = glGetUniformLocation(prog, "transforms");
    if (loc != -1) glUniform1i(loc, 1);
    loc = glGetUniformLocation(prog, "instanceColors");
    if (loc != -1) glUniform1i(loc, 2);

    drawable->drawImplementation(renderInfo);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
}

// -------------------------------------------------------
// HardwareInstancing
// -------------------------------------------------------
HardwareInstancing::HardwareInstancing(const Config& cfg)
    : m_cfg(cfg)
    , m_instanceCount(cfg.gridX * cfg.gridZ)
{}

HardwareInstancing::~HardwareInstancing()
{
    // m_colors va m_dirty da duoc chuyen ownership sang
    // TransformBufferCallback sau khi createScene() duoc goi.
    // Chi delete neu createScene() chua duoc goi (ownership chua chuyen).
    if (m_colors) { delete m_colors; m_colors = nullptr; }
    if (m_dirty)  { delete m_dirty;  m_dirty  = nullptr; }
}

// -------------------------------------------------------
// Box geometry: 24 verts, 36 indices, normals per vertex
// -------------------------------------------------------
osg::ref_ptr<osg::Geometry> HardwareInstancing::createBoxGeometry()
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseVertexBufferObjects(true);
    geom->setUseDisplayList(false);
    geom->setDataVariance(osg::Object::STATIC);

    osg::ref_ptr<osg::Vec3Array> verts   = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();

    struct Face { osg::Vec3 n; osg::Vec3 v[4]; };
    static const Face faces[6] = {
        // +X
        { {1,0,0},  { {.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f},{.5f,-.5f,.5f} } },
        // -X
        { {-1,0,0}, { {-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,-.5f} } },
        // +Y
        { {0,1,0},  { {-.5f,.5f,-.5f},{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f} } },
        // -Y
        { {0,-1,0}, { {-.5f,-.5f,.5f},{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f} } },
        // +Z
        { {0,0,1},  { {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f} } },
        // -Z
        { {0,0,-1}, { {.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f} } },
    };

    for (const auto& face : faces)
        for (int i = 0; i < 4; ++i)
        {
            verts->push_back(face.v[i]);
            normals->push_back(face.n);
        }

    osg::ref_ptr<osg::DrawElementsUShort> idx =
        new osg::DrawElementsUShort(GL_TRIANGLES);
    for (int f = 0; f < 6; ++f)
    {
        int b = f * 4;
        idx->push_back(b);   idx->push_back(b+1); idx->push_back(b+2);
        idx->push_back(b);   idx->push_back(b+2); idx->push_back(b+3);
    }
    idx->setNumInstances(m_instanceCount); // key cho hardware instancing

    geom->setVertexArray(verts);
    geom->setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(idx);

    return geom;
}

// -------------------------------------------------------
// Generate per-instance mat4 (float) + color
// Giong logic trong makeHardwareInstancing: dung Matrixf,
// tach local origin de tranh float precision loss
// -------------------------------------------------------
void HardwareInstancing::generateInstanceData()
{
    m_matrices.resize(m_instanceCount);
    m_colors = new std::vector<osg::Vec4>(m_instanceCount);

    srand(m_cfg.seed);
    auto randf = [](float lo, float hi) -> float {
        return lo + (hi - lo) * (rand() / (float)RAND_MAX);
    };

    float totalW = (m_cfg.gridX - 1) * m_cfg.spacing;
    float totalD = (m_cfg.gridZ - 1) * m_cfg.spacing;

    for (int zi = 0; zi < m_cfg.gridZ; ++zi)
    {
        for (int xi = 0; xi < m_cfg.gridX; ++xi)
        {
            int i = zi * m_cfg.gridX + xi;

            float sx = randf(m_cfg.minScale,  m_cfg.maxScale);
            float sy = randf(m_cfg.minHeight, m_cfg.maxHeight);
            float sz = randf(m_cfg.minScale,  m_cfg.maxScale);

            float px = xi * m_cfg.spacing - totalW * 0.5f;
            float pz = zi * m_cfg.spacing - totalD * 0.5f;
            float py = sy * 0.5f; // day box nam tren y=0

            // Scale + Translate (column-major, giong osg::Matrixf layout)
            osg::Matrixf mat = osg::Matrixf::scale(sx, sy, sz)
                             * osg::Matrixf::translate(px / sx, py / sy, pz / sz);
            // Cach don gian hon: set truc tiep
            mat = osg::Matrixf(
                sx,   0,    0,    0,
                0,    sy,   0,    0,
                0,    0,    sz,   0,
                px,   py,   pz,   1
            );

            m_matrices[i] = mat;

            // Random pastel color
            (*m_colors)[i] = osg::Vec4(
                randf(0.35f, 1.0f),
                randf(0.35f, 1.0f),
                randf(0.35f, 1.0f),
                1.0f
            );
        }
    }

    // Luu ban goc de reset
    m_savedColors = *m_colors;
    m_dirty = new int(0);

    std::cout << "[HardwareInstancing] Generated " << m_instanceCount
              << " instances\n";
}

// -------------------------------------------------------
// Tinh union BBox cua tat ca instances
// -------------------------------------------------------
osg::BoundingBox HardwareInstancing::computeUnionBBox(
    const osg::BoundingBox& localBB) const
{
    osg::BoundingBox unionBB;
    for (const auto& mat : m_matrices)
    {
        for (int c = 0; c < 8; ++c)
        {
            osg::Vec3 corner(
                (c & 1) ? localBB.xMax() : localBB.xMin(),
                (c & 2) ? localBB.yMax() : localBB.yMin(),
                (c & 4) ? localBB.zMax() : localBB.zMin()
            );
            unionBB.expandBy(osg::Vec3f(mat.preMult(corner)));
        }
    }
    return unionBB;
}

// -------------------------------------------------------
// Shader program - dung #version 330 compatibility
// -------------------------------------------------------
osg::ref_ptr<osg::Program> HardwareInstancing::createShaderProgram()
{
    osg::ref_ptr<osg::Program> prog = new osg::Program();
    prog->setName("HardwareInstancing");
    prog->addShader(new osg::Shader(osg::Shader::VERTEX,   s_vertSrc));
    prog->addShader(new osg::Shader(osg::Shader::FRAGMENT, s_fragSrc));
    return prog;
}

// -------------------------------------------------------
// createScene: entry point chinh
// Giong flow trong makeHardwareInstancing:
// 1. Generate data
// 2. Clone / setup geometry
// 3. setNumInstances tren primitive set
// 4. Attach DrawCallback
// 5. Setup shader + uniforms
// 6. Fix bounding box
// -------------------------------------------------------
osg::ref_ptr<osg::Group> HardwareInstancing::createScene()
{
    generateInstanceData();

    osg::ref_ptr<osg::Geometry> geom = createBoxGeometry();

    // --- 1. setNumInstances (da set trong createBoxGeometry) ---

    // --- 2. Attach DrawCallback ---
    // Sau khi tao callback, release ownership khoi HardwareInstancing
    // de destructor khong delete m_colors/m_dirty (callback dang dung)
    std::vector<osg::Vec4>* colorsPtr = m_colors;
    int*                    dirtyPtr  = m_dirty;
    m_colors = nullptr;  // transfer ownership sang callback
    m_dirty  = nullptr;

    osg::ref_ptr<TransformBufferCallback> cb =
        new TransformBufferCallback(m_matrices, colorsPtr, dirtyPtr);
    geom->setDrawCallback(cb);

    // --- 3. Shader + uniforms ---
    osg::ref_ptr<osg::StateSet> ss = geom->getOrCreateStateSet();
    ss->setAttribute(createShaderProgram(), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    // --- 4. Fix bounding box ---
    osg::BoundingBox localBB = geom->getBoundingBox();
    osg::BoundingBox unionBB = computeUnionBBox(localBB);
    // Expand 10% de tranh bi cull o bien
    osg::Vec3 ext = (unionBB._max - unionBB._min) * 0.1f;
    unionBB._min -= ext;
    unionBB._max += ext;

    geom->setComputeBoundingBoxCallback(new InstancedBBoxCallback(unionBB));
    geom->dirtyBound();
    geom->setCullingActive(false);

    // --- 5. Geode + Group ---
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geom);
    geode->setCullingActive(false);

    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->addChild(geode);

    std::cout << "[HardwareInstancing] Scene ready: "
              << m_instanceCount << " instances\n";

    return root;
}

// -------------------------------------------------------
// Color update (giong highlight/hover trong production)
// -------------------------------------------------------
void HardwareInstancing::setInstanceColor(int index, const osg::Vec4& color)
{
    if (!m_colors || index < 0 || index >= m_instanceCount) return;
    (*m_colors)[index] = color;
    *m_dirty = 1;
}

void HardwareInstancing::resetInstanceColor(int index)
{
    if (!m_colors || index < 0 || index >= m_instanceCount) return;
    (*m_colors)[index] = m_savedColors[index];
    *m_dirty = 1;
}
