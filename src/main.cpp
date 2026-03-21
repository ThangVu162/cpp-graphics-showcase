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

#include "HardwareInstancing.h"
#include "SystemMonitor.h"

// -------------------------------------------------------
// Naive scene: moi object = 1 ShapeDrawable rieng
// De thay su khac biet FPS voi instancing
// -------------------------------------------------------
static osg::ref_ptr<osg::Group> createNaiveScene(
    const HardwareInstancing::Config& cfg)
{
    osg::ref_ptr<osg::Group> root = new osg::Group();

    srand(cfg.seed);
    auto randf = [](float lo, float hi) -> float {
        return lo + (hi - lo) * (rand() / (float)RAND_MAX);
        };

    float totalW = (cfg.gridX - 1) * cfg.spacing;
    float totalD = (cfg.gridZ - 1) * cfg.spacing;

    for (int zi = 0; zi < cfg.gridZ; ++zi)
        for (int xi = 0; xi < cfg.gridX; ++xi)
        {
            float sx = randf(cfg.minScale, cfg.maxScale);
            float sy = randf(cfg.minHeight, cfg.maxHeight);
            float sz = randf(cfg.minScale, cfg.maxScale);
            float px = xi * cfg.spacing - totalW * 0.5f;
            float pz = zi * cfg.spacing - totalD * 0.5f;
            float py = sy * 0.5f;

            osg::ref_ptr<osg::Box> box =
                new osg::Box(osg::Vec3(px, py, pz), sx, sy, sz);
            osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(box);
            sd->setColor(osg::Vec4(
                randf(0.35f, 1.f),
                randf(0.35f, 1.f),
                randf(0.35f, 1.f), 1.f));

            osg::ref_ptr<osg::Geode> geode = new osg::Geode();
            geode->addDrawable(sd);
            root->addChild(geode);
        }

    std::cout << "[NaiveScene] " << cfg.gridX * cfg.gridZ
        << " separate draw calls (no instancing)\n";
    return root;
}

// -------------------------------------------------------
// HUD
// -------------------------------------------------------
struct HUDData
{
    osg::ref_ptr<osgText::Text> title;
    osg::ref_ptr<osgText::Text> fps;
    osg::ref_ptr<osgText::Text> sys;    // RAM + CPU + GPU
    osg::ref_ptr<osgText::Text> status;
    osg::ref_ptr<osgText::Text> hint;
};

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
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
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

    out.title = makeText(10, h - 28.f, 18, { 1,1,0,1 },
        "C++ Graphics Showcase | Phase 2: Hardware Instancing (TBO)");
    out.fps = makeText(10, h - 52.f, 16, { .4f,1,.4f,1 },
        "FPS: --");
    out.sys = makeText(10, h - 72.f, 13, { .8f,.8f,.8f,1 },
        "RAM: -- MB   CPU: -- %");
    out.status = makeText(10, h - 92.f, 15, { .9f,.9f,1,1 },
        "");
    out.hint = makeText(10, 10.f, 13, { .75f,.75f,.75f,1 },
        "[+]/[-] Grid size  |  [I] Toggle Instancing  |  [Enter] Reload  |  [S] Stats  |  [Esc] Quit");

    hud->addChild(geode);
    return hud;
}


// -------------------------------------------------------
// AppState
// -------------------------------------------------------
struct AppState
{
    HardwareInstancing::Config cfg;
    bool useInstancing = true;
    bool dirty = false;

    osg::ref_ptr<osg::Group> sceneRoot;    // geometry node (swap khi reload)
    osg::ref_ptr<osg::Group> masterRoot;   // root chinh
    HUDData                  hud;

    void updateStatus()
    {
        std::ostringstream oss;
        if (useInstancing)
            oss << "[INSTANCING ON]  ";
        else
            oss << "[INSTANCING OFF - Naive draw calls]  ";

        oss << "Grid: " << cfg.gridX << "x" << cfg.gridZ
            << " = " << cfg.gridX * cfg.gridZ << " objects";

        if (dirty)
            oss << "   <<< Press [Enter] to apply >>>";

        hud.status->setText(oss.str());

        // Warna status: hijau = instancing, kuning = naive
        if (useInstancing)
            hud.status->setColor(osg::Vec4(.5f, 1.f, .5f, 1.f));
        else
            hud.status->setColor(osg::Vec4(1.f, .8f, .3f, 1.f));
    }
};

// -------------------------------------------------------
// Event handler: +/- grid, I toggle, Enter apply
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

        // Tang grid
        if (key == '+' || key == '=')
        {
            m_state->cfg.gridX = min(m_state->cfg.gridX + 50, 1000);
            m_state->cfg.gridZ = min(m_state->cfg.gridZ + 50, 1000);
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        // Giam grid
        if (key == '-' || key == '_')
        {
            m_state->cfg.gridX = max(m_state->cfg.gridX - 10, 10);
            m_state->cfg.gridZ = max(m_state->cfg.gridZ - 10, 10);
            m_state->dirty = true;
            m_state->updateStatus();
            return true;
        }
        // Toggle instancing
        if (key == 'i' || key == 'I')
        {
            m_state->useInstancing = !m_state->useInstancing;
            m_state->dirty = true;
            m_state->updateStatus();
            std::cout << "[Demo] Instancing: "
                << (m_state->useInstancing ? "ON" : "OFF") << "\n";
            return true;
        }
        // Apply
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
// Rebuild scene (swap sceneRoot trong masterRoot)
// -------------------------------------------------------
static void rebuildScene(AppState& state)
{
    if (state.sceneRoot.valid())
        state.masterRoot->removeChild(state.sceneRoot);

    if (state.useInstancing)
    {
        HardwareInstancing inst(state.cfg);
        state.sceneRoot = inst.createScene();
    }
    else
    {
        state.sceneRoot = createNaiveScene(state.cfg);
    }

    state.masterRoot->addChild(state.sceneRoot);
    state.dirty = false;
    state.updateStatus();

    std::cout << "[Demo] Rebuilt: "
        << (state.useInstancing ? "INSTANCING" : "NAIVE")
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
    state.cfg.gridX = 100;
    state.cfg.gridZ = 100;
    state.cfg.spacing = 2.5f;
    state.cfg.minScale = 0.3f;
    state.cfg.maxScale = 1.2f;
    state.cfg.minHeight = 0.5f;
    state.cfg.maxHeight = 3.5f;
    state.cfg.seed = 12345u;
    state.useInstancing = true;

    // Viewer
    osgViewer::Viewer viewer;
    viewer.setUpViewInWindow(100, 100, 1280, 720);
    viewer.getCamera()->setClearColor(osg::Vec4(0.12f, 0.12f, 0.14f, 1.f));

    // Master root
    state.masterRoot = new osg::Group();

    // HUD
    osg::ref_ptr<osg::Camera> hudCam = createHUD(1280, 720, state.hud);
    hudCam->setUpdateCallback(
        new MonitorCallback(state.hud.fps, state.hud.sys, &viewer));
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
        osg::Vec3(0, 0, 0),
        osg::Vec3(0, 0, 1));
    viewer.setCameraManipulator(manip);
    viewer.setSceneData(state.masterRoot);
    viewer.home();

    std::cout << "Controls:\n"
        << "  [+]/[-]  : Grid size +/- 10\n"
        << "  [I]      : Toggle hardware instancing\n"
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