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
    osg::ref_ptr<osgText::Text> flags;   // trang thai ON/OFF cua tung optimization
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
    out.flags  = makeText(10, h-112.f, 14, {1,1,1,1}, "");

    out.hint   = makeText(10, 10.f, 12, {.65f,.65f,.65f,1},
                    "[+]/[-] Grid  [M] Mode  [F] Frustum  [L] LOD  [P] SmallFeat  [V] Wireframe  [Enter] Reload  [S] Stats");

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
    MonitorCallback*                   monitorCb    = nullptr;
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
        _applyFlags();
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

    void _applyFlags()
    {
        if (mode != 1 || !chunker)
        {
            // Che flags row khi khong o Chunking mode
            hud.flags->setText("");
            return;
        }

        bool fc = chunker->getFrustumCulling();
        bool lod = chunker->getLOD();
        bool sf = chunker->getSmallFeatureCulling();

        // Tao rich-text gia: dung nhieu Text nodes hoac 1 string voi ky hieu
        // Vi OSG osgText khong ho tro mixed color trong 1 Text node,
        // nen dung ky hieu de phan biet ON/OFF
        std::string txt =
            "[F] Frustum "    + std::string(fc  ? "■ON " : "□OFF") + "   " +
            "[L] LOD "        + std::string(lod ? "■ON " : "□OFF") + "   " +
            "[P] SmallFeat "  + std::string(sf  ? "■ON " : "□OFF") + "   " +
            "[V] Wireframe "  + std::string(showWireframe ? "■ON" : "□OFF");

        hud.flags->setText(txt);

        // Mau tong the: xanh neu tat ca ON, vang neu 1 so OFF, do neu tat ca OFF
        int onCount = (fc?1:0) + (lod?1:0) + (sf?1:0) + (showWireframe?1:0);
        if (onCount == 4)
            hud.flags->setColor({.4f,1.f,.4f,1.f});   // tat ca ON → xanh
        else if (onCount == 0)
            hud.flags->setColor({1.f,.4f,.4f,1.f});   // tat ca OFF → do
        else
            hud.flags->setColor({1.f,.85f,.3f,1.f});  // 1 so OFF → vang
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
            m_state->cfg.gridX = min(m_state->cfg.gridX + 10, 500);
            m_state->cfg.gridZ = min(m_state->cfg.gridZ + 10, 500);
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        if (key == '-' || key == '_')
        {
            m_state->cfg.gridX = max(m_state->cfg.gridX - 10, 10);
            m_state->cfg.gridZ = max(m_state->cfg.gridZ - 10, 10);
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

    if (state.mode == 0)
    {
        HardwareInstancing inst(state.cfg);
        state.sceneRoot = inst.createScene();
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
