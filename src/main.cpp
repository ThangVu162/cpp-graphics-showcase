// main.cpp - OSGViewer Phase 2: Hardware Instancing
// Controls:
//   [+] / [-]   : Tang/giam grid size (buoc 10)
//   [I]         : Toggle hardware instancing ON/OFF
//   [Enter]     : Apply thay doi, reload scene
//   [S]         : Stats/FPS overlay
//   [Esc]       : Quit

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osgGA/TrackballManipulator>
#include <osgText/Text>

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <memory>

#include "HardwareInstancing.h"
#include "SystemMonitor.h"
#include "GeometryChunker.h"
#include "NaiveScene.h"
#include "BVHPicker.h"

// NaiveScene duoc quan ly boi NaiveScene.h

// -------------------------------------------------------
// HUD
// -------------------------------------------------------
struct HUDData
{
    osg::ref_ptr<osgText::Text> title;
    osg::ref_ptr<osgText::Text> fps;
    osg::ref_ptr<osgText::Text> sys;
    osg::ref_ptr<osgText::Text> status;
    osg::ref_ptr<osgText::Text> flags;
    osg::ref_ptr<osgText::Text> pickInfo; // ket qua pick
    osg::ref_ptr<osgText::Text> hint;
};

// Helper: tao 1 text badge "[ON]" xanh hoac "[OFF]" do
static void updateFlag(osgText::Text* t, const std::string& label, bool on)
{
    t->setText(label + (on ? " [ON]" : " [OFF]"));
    t->setColor(on ? osg::Vec4(.3f,1.f,.3f,1.f)   // xanh la = ON
                   : osg::Vec4(1.f,.35f,.35f,1.f)); // do = OFF
}

static osg::ref_ptr<osg::Camera> createHUD(int w, int h, HUDData& out)
{
    osg::ref_ptr<osg::Camera> hud = new osg::Camera();
    hud->setProjectionMatrix(osg::Matrix::ortho2D(0, w, 0, h));
    hud->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hud->setViewMatrix(osg::Matrix::identity());
    hud->setClearMask(GL_DEPTH_BUFFER_BIT);
    hud->setRenderOrder(osg::Camera::POST_RENDER);
    hud->setAllowEventFocus(false);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING,   osg::StateAttribute::OFF);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

    auto makeText = [&](float x, float y, float sz,
                        const osg::Vec4& col,
                        const std::string& str) -> osg::ref_ptr<osgText::Text>
    {
        osg::ref_ptr<osgText::Text> t = new osgText::Text();
        t->setFont("fonts/arial.ttf");
        t->setCharacterSize(sz);
        t->setPosition(osg::Vec3(x, y, 0));
        t->setColor(col);
        t->setText(str);
        t->setDataVariance(osg::Object::DYNAMIC);
        geode->addDrawable(t);
        return t;
    };

    out.title  = makeText(10, h-28.f, 18, {1,1,0,1},
                    "C++ Graphics Showcase | Phase 3: Chunking + LOD + Culling");
    out.fps    = makeText(10, h-52.f, 16, {.4f,1,.4f,1},
                    "FPS: --");
    out.sys    = makeText(10, h-72.f, 13, {.8f,.8f,.8f,1},
                    "RAM: -- MB   CPU: -- %");
    out.status = makeText(10, h-92.f, 15, {.9f,.9f,1,1},
                    "");

    // Flags row: hien thi trang thai tung optimization
    // Chi visible khi o Chunking mode, nen de mau trang ban dau
    out.flags    = makeText(10, h-112.f, 14, {1,1,1,1}, "");
    out.pickInfo = makeText(10, h-132.f, 14, {1,.8f,.2f,1}, ""); // vàng
    out.hint     = makeText(10, 10.f, 12, {.65f,.65f,.65f,1},
        "[LClick] Pick  [+]/[-] Grid  [M] Mode  [F] Frustum  [L] LOD  [P] SmallFeat  [Enter] Reload  [S] Stats");

    hud->addChild(geode);
    return hud;
}


// -------------------------------------------------------
// AppState
// -------------------------------------------------------
struct AppState
{
    HardwareInstancing::Config cfg;

    // 0 = Instancing, 1 = Chunking, 2 = Naive
    int  mode  = 0;
    bool dirty = false;
    bool showWireframe = true;

    ChunkStats*                        chunkStats   = nullptr;
    std::unique_ptr<GeometryChunker>   chunker;
    std::unique_ptr<NaiveScene>        naiveScene;
    std::unique_ptr<HardwareInstancing> instancing;  // cho picking
    MonitorCallback*                   monitorCb    = nullptr;
    int                                pickedIndex  = -1; // instance dang highlight
    osg::ref_ptr<osg::Group> sceneRoot;
    osg::ref_ptr<osg::Group> masterRoot;
    HUDData                  hud;

    std::string modeName() const
    {
        if (mode == 0) return "[INSTANCING]";
        if (mode == 1) return "[CHUNKING + Frustum Cull]";
        return "[NAIVE draw calls]";
    }

    void updateStatus()
    {
        std::ostringstream oss;
        oss << modeName()
            << "  Grid: " << cfg.gridX << "x" << cfg.gridZ
            << " = " << cfg.gridX * cfg.gridZ << " objects";
        if (dirty) oss << "  <<< [Enter] to apply >>>";
        _applyStatus(oss.str());
        // flags row duoc cap nhat moi frame qua MonitorCallback lambda
    }

private:
    void _applyStatus(const std::string& str)
    {
        hud.status->setText(str);
        if (mode == 0)
            hud.status->setColor({.5f,1.f,.5f,1.f});
        else if (mode == 1)
            hud.status->setColor({.5f,.8f,1.f,1.f});
        else
            hud.status->setColor({1.f,.8f,.3f,1.f});
    }
public:
};

// -------------------------------------------------------
// Event handler
// [M] cycle mode, [V] wireframe, [+/-] grid, [Enter] apply
// -------------------------------------------------------
class DemoEventHandler : public osgGA::GUIEventHandler
{
public:
    DemoEventHandler(AppState* s) : m_state(s) {}

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter&) override
    {
        if (ea.getEventType() != osgGA::GUIEventAdapter::KEYDOWN)
            return false;

        int key = ea.getKey();

        if (key == '+' || key == '=')
        {
            m_state->cfg.gridX = min(m_state->cfg.gridX + 50, 1000);
            m_state->cfg.gridZ = min(m_state->cfg.gridZ + 50, 1000);
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        if (key == '-' || key == '_')
        {
            m_state->cfg.gridX = max(m_state->cfg.gridX - 50, 10);
            m_state->cfg.gridZ = max(m_state->cfg.gridZ - 50, 10);
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        // [M] cycle: Instancing(0) → Chunking(1) → Naive(2)
        if (key == 'm' || key == 'M')
        {
            m_state->mode  = (m_state->mode + 1) % 3;
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        // [V] toggle wireframe chunk bounds
        if (key == 'v' || key == 'V')
        {
            m_state->showWireframe = !m_state->showWireframe;
            m_state->dirty = true;
            return true;
        }
        // [F] Frustum Culling
        if (key == 'f' || key == 'F')
        {
            if (m_state->chunker) {
                m_state->chunker->setFrustumCulling(
                    !m_state->chunker->getFrustumCulling());
            } else if (m_state->naiveScene) {
                m_state->naiveScene->setFrustumCulling(
                    !m_state->naiveScene->getFrustumCulling());
            }
            m_state->updateStatus();
            return true;
        }
        // [L] LOD
        if (key == 'l' || key == 'L')
        {
            if (m_state->chunker) {
                m_state->chunker->setLOD(!m_state->chunker->getLOD());
            } else if (m_state->naiveScene) {
                m_state->naiveScene->setLOD(!m_state->naiveScene->getLOD());
            }
            m_state->updateStatus();
            return true;
        }
        // [P] Small Feature Culling
        if (key == 'p' || key == 'P')
        {
            if (m_state->chunker) {
                m_state->chunker->setSmallFeatureCulling(
                    !m_state->chunker->getSmallFeatureCulling());
            } else if (m_state->naiveScene) {
                m_state->naiveScene->setSmallFeatureCulling(
                    !m_state->naiveScene->getSmallFeatureCulling());
            }
            m_state->updateStatus();
            return true;
        }
        if (key == osgGA::GUIEventAdapter::KEY_Return ||
            key == osgGA::GUIEventAdapter::KEY_KP_Enter)
        {
            m_state->dirty = true;
            return true;
        }
        return false;
    }

private:
    AppState* m_state;
};

// -------------------------------------------------------
// Rebuild scene
// -------------------------------------------------------
static void rebuildScene(AppState& state)
{
    if (state.sceneRoot.valid())
        state.masterRoot->removeChild(state.sceneRoot);

    state.chunkStats = nullptr;
    state.chunker.reset();
    state.naiveScene.reset();
    state.instancing.reset();
    state.pickedIndex = -1;
    state.hud.pickInfo->setText("");

    if (state.mode == 0)
    {
        state.instancing = std::make_unique<HardwareInstancing>(state.cfg);
        state.sceneRoot  = state.instancing->createScene();
    }
    else if (state.mode == 1)
    {
        state.chunker    = std::make_unique<GeometryChunker>(state.cfg);
        state.sceneRoot  = state.chunker->createScene(state.showWireframe);
        state.chunkStats = state.chunker->getStats();
    }
    else
    {
        state.naiveScene = std::make_unique<NaiveScene>(state.cfg);
        state.sceneRoot  = state.naiveScene->createScene();
    }

    if (state.monitorCb)
        state.monitorCb->setChunkStats(state.chunkStats);

    state.masterRoot->addChild(state.sceneRoot);
    state.dirty = false;
    state.updateStatus();

    std::cout << "[Demo] Rebuilt: " << state.modeName()
              << " | " << state.cfg.gridX * state.cfg.gridZ << " objects\n";
}

// -------------------------------------------------------
// PickEventHandler: Left click → BVH pick → highlight
// -------------------------------------------------------
class PickEventHandler : public osgGA::GUIEventHandler
{
public:
    PickEventHandler(AppState* s, osgViewer::Viewer* v)
        : m_state(s), m_viewer(v) {}

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter&) override
    {
        // Chi xu ly left mouse button release
        if (ea.getEventType() != osgGA::GUIEventAdapter::RELEASE) return false;
        if (ea.getButton()    != osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) return false;
        if (!m_state->instancing) return false; // chi pick o Instancing mode

        BVHPicker* picker = m_state->instancing->getPicker();
        if (!picker) return false;

        // Tinh ray tu mouse position
        osg::Camera* cam = m_viewer->getCamera();
        osg::Matrix VP  = cam->getViewMatrix() * cam->getProjectionMatrix();
        osg::Matrix invVP;
        invVP.invert(VP);

        // NDC → world
        float mx = ea.getXnormalized();
        float my = ea.getYnormalized();
        osg::Vec3d nearPt = osg::Vec3d(mx, my, -1.0) * invVP;
        osg::Vec3d farPt  = osg::Vec3d(mx, my,  1.0) * invVP;
        osg::Vec3d rayDir = farPt - nearPt;
        rayDir.normalize();

        // Reset highlight cu
        if (m_state->pickedIndex >= 0)
        {
            m_state->instancing->resetInstanceColor(m_state->pickedIndex);
            picker->hideHighlight();
        }

        // Pick
        BVHPicker::PickResult result = picker->pick(nearPt, rayDir);

        if (result.instanceIndex >= 0)
        {
            // Doi mau instance → vang
            m_state->instancing->setInstanceColor(
                result.instanceIndex, osg::Vec4(1.f, 0.9f, 0.1f, 1.f));

            // Hien wireframe highlight box
            picker->showHighlight(result.instanceIndex);

            m_state->pickedIndex = result.instanceIndex;

            const osg::Vec3d& p = result.hitPoint;
            char buf[128];
            snprintf(buf, sizeof(buf),
                "Pick: Instance #%d  |  Dist: %.1f  |  Pos: (%.1f, %.1f, %.1f)",
                result.instanceIndex, result.distance,
                p.x(), p.y(), p.z());
            m_state->hud.pickInfo->setText(buf);
            std::cout << buf << "\n";
        }
        else
        {
            picker->hideHighlight();
            m_state->pickedIndex = -1;
            m_state->hud.pickInfo->setText("Pick: no hit");
        }

        return false; // khong consume event de camera van hoat dong
    }

private:
    AppState*          m_state;
    osgViewer::Viewer* m_viewer;
};

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main(int argc, char** argv)
{
    osg::setNotifyLevel(osg::WARN);

    // Config mac dinh
    AppState state;
    state.cfg.gridX     = 100;
    state.cfg.gridZ     = 100;
    state.cfg.spacing   = 2.5f;
    state.cfg.minScale  = 0.3f;
    state.cfg.maxScale  = 1.2f;
    state.cfg.minHeight = 0.5f;
    state.cfg.maxHeight = 3.5f;
    state.cfg.seed      = 12345u;
    state.mode          = 0;
    state.showWireframe = true;

    // Viewer
    osgViewer::Viewer viewer;
    viewer.setUpViewInWindow(100, 100, 1280, 720);
    viewer.getCamera()->setClearColor(osg::Vec4(0.12f, 0.12f, 0.14f, 1.f));

    // Master root
    state.masterRoot = new osg::Group();

    // HUD
    osg::ref_ptr<osg::Camera> hudCam = createHUD(1280, 720, state.hud);
    MonitorCallback* monCb = new MonitorCallback(
        state.hud.fps, state.hud.sys, &viewer);
    state.monitorCb = monCb;

    // Per-frame callback: cap nhat flags row moi frame
    monCb->setPerFrameCallback([&state]()
    {
        auto badge = [](bool on) -> std::string {
            return on ? "[ON]  " : "[OFF] ";
        };

        std::string txt;

        if (state.mode == 0) // Instancing
        {
            // Instancing khong co culling/LOD
            // Hien thi de contrast voi Chunking
            txt = "[F] Frustum [N/A]  "
                  "[L] LOD [N/A]  "
                  "[P] SmallFeat [N/A]  "
                  "| 1 draw call, no per-instance cull";
            state.hud.flags->setColor({.6f,.6f,.6f,1.f}); // xam
        }
        else if (state.mode == 1 && state.chunker) // Chunking
        {
            bool fc  = state.chunker->getFrustumCulling();
            bool lod = state.chunker->getLOD();
            bool sf  = state.chunker->getSmallFeatureCulling();
            bool wf  = state.showWireframe;

            txt = "[F] Frustum "   + badge(fc)
                + "[L] LOD "       + badge(lod)
                + "[P] SmallFeat " + badge(sf)
                + "[V] Wireframe " + badge(wf);

            int on = (fc?1:0)+(lod?1:0)+(sf?1:0)+(wf?1:0);
            if      (on == 4) state.hud.flags->setColor({.35f,1.f,.35f,1.f});
            else if (on == 0) state.hud.flags->setColor({1.f,.35f,.35f,1.f});
            else              state.hud.flags->setColor({1.f,.85f,.3f, 1.f});
        }
        else if (state.mode == 2 && state.naiveScene) // Naive với optimizations
        {
            bool fc  = state.naiveScene->getFrustumCulling();
            bool lod = state.naiveScene->getLOD();
            bool sf  = state.naiveScene->getSmallFeatureCulling();

            txt = "[F] Frustum "   + badge(fc)
                + "[L] LOD "       + badge(lod)
                + "[P] SmallFeat " + badge(sf)
                + "| " + std::to_string(state.cfg.gridX * state.cfg.gridZ)
                + " draw calls";

            int on = (fc?1:0)+(lod?1:0)+(sf?1:0);
            if      (on == 3) state.hud.flags->setColor({.35f,1.f,.35f,1.f});
            else if (on == 0) state.hud.flags->setColor({1.f,.35f,.35f,1.f});
            else              state.hud.flags->setColor({1.f,.85f,.3f, 1.f});
        }
        else // fallback
        {
            // OSG tu dong frustum cull moi Geode, khong co LOD
            txt = "[F] Frustum [ON]  "  // OSG auto, khong tat duoc
                  "[L] LOD [N/A]  "
                  "[P] SmallFeat [N/A]  "
                  "| " + std::to_string(state.cfg.gridX * state.cfg.gridZ)
                + " draw calls";
            state.hud.flags->setColor({1.f,.8f,.3f,1.f}); // vang
        }

        state.hud.flags->setText(txt);
    });

    hudCam->setUpdateCallback(monCb);
    state.masterRoot->addChild(hudCam);

    // Build scene lan dau
    rebuildScene(state);

    // Event handlers
    viewer.addEventHandler(new DemoEventHandler(&state));
    viewer.addEventHandler(new PickEventHandler(&state, &viewer));
    viewer.addEventHandler(new osgViewer::StatsHandler());

    // Camera
    osg::ref_ptr<osgGA::TrackballManipulator> manip =
        new osgGA::TrackballManipulator();
    manip->setHomePosition(
        osg::Vec3(0, -200, 120),
        osg::Vec3(0,    0,   0),
        osg::Vec3(0,    0,   1));
    viewer.setCameraManipulator(manip);
    viewer.setSceneData(state.masterRoot);
    viewer.home();

    std::cout << "Controls:\n"
              << "  [+]/[-]  : Grid size\n"
              << "  [M]      : Cycle mode (Instancing/Chunking/Naive)\n"
              << "  [V]      : Toggle wireframe (Chunking mode)\n"
              << "  [F]      : Toggle Frustum Culling (Chunking mode)\n"
              << "  [L]      : Toggle LOD (Chunking mode)\n"
              << "  [P]      : Toggle Small Feature Culling (Chunking mode)\n"
              << "  [Enter]  : Apply & reload\n"
              << "  [S]      : FPS stats\n"
              << "  [Esc]    : Quit\n\n";

    // Main loop: check dirty flag moi frame
    while (!viewer.done())
    {
        if (state.dirty)
            rebuildScene(state);

        viewer.frame();
    }

    return 0;
}
