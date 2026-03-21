// main.cpp - OSGViewer Phase 1: Basic setup test
// Confirms OSG linkage, window creation, basic geometry render

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osg/Geode>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/LightModel>
#include <osgGA/TrackballManipulator>
#include <osgText/Text>
#include <iostream>

// -------------------------------------------------------
// Tao 1 box don gian de test render
// -------------------------------------------------------
osg::ref_ptr<osg::Node> createTestBox()
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();

    // Box 1x1x1 tai goc toa do
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0, 0, 0), 1.0f);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box);
    drawable->setColor(osg::Vec4(0.2f, 0.6f, 1.0f, 1.0f)); // xanh duong

    geode->addDrawable(drawable);

    // Material
    osg::ref_ptr<osg::Material> mat = new osg::Material();
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.2f, 0.6f, 1.0f, 1.0f));
    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.1f, 0.1f, 1.0f));
    mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    mat->setShininess(osg::Material::FRONT_AND_BACK, 64.0f);
    geode->getOrCreateStateSet()->setAttribute(mat);

    return geode;
}

// -------------------------------------------------------
// HUD text hien thi thong tin
// -------------------------------------------------------
osg::ref_ptr<osg::Camera> createHUD(int width, int height)
{
    osg::ref_ptr<osg::Camera> hud = new osg::Camera();
    hud->setProjectionMatrix(osg::Matrix::ortho2D(0, width, 0, height));
    hud->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hud->setViewMatrix(osg::Matrix::identity());
    hud->setClearMask(GL_DEPTH_BUFFER_BIT);
    hud->setRenderOrder(osg::Camera::POST_RENDER);
    hud->setAllowEventFocus(false);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();

    osg::ref_ptr<osgText::Text> text = new osgText::Text();
    text->setFont("fonts/arial.ttf");
    text->setCharacterSize(18.0f);
    text->setPosition(osg::Vec3(10.0f, height - 30.0f, 0.0f));
    text->setColor(osg::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    text->setText("OSGViewer Demo | Phase 1: Basic Setup OK");
    text->setDataVariance(osg::Object::DYNAMIC);

    osg::ref_ptr<osgText::Text> hint = new osgText::Text();
    hint->setFont("fonts/arial.ttf");
    hint->setCharacterSize(14.0f);
    hint->setPosition(osg::Vec3(10.0f, 10.0f, 0.0f));
    hint->setColor(osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
    hint->setText("Left drag: Rotate | Middle drag: Pan | Scroll: Zoom");

    geode->addDrawable(text);
    geode->addDrawable(hint);

    // Tat lighting cho HUD
    geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    geode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

    hud->addChild(geode);
    return hud;
}

int main(int argc, char** argv)
{
    std::cout << "[OSGViewer] Starting Phase 1 test..." << std::endl;

    // ---- Viewer setup ----
    osgViewer::Viewer viewer;
    viewer.setUpViewInWindow(100, 100, 1280, 720);
    viewer.getCamera()->setClearColor(osg::Vec4(0.15f, 0.15f, 0.15f, 1.0f));

    // ---- Scene root ----
    osg::ref_ptr<osg::Group> root = new osg::Group();

    // Two-sided lighting
    osg::ref_ptr<osg::LightModel> lm = new osg::LightModel();
    lm->setTwoSided(true);
    root->getOrCreateStateSet()->setAttribute(lm);

    // Scene geometry
    root->addChild(createTestBox());

    // HUD
    root->addChild(createHUD(1280, 720));

    // ---- Stats handler (F key de hien FPS) ----
    viewer.addEventHandler(new osgViewer::StatsHandler());

    // ---- Camera manipulator ----
    osg::ref_ptr<osgGA::TrackballManipulator> manip = new osgGA::TrackballManipulator();
    viewer.setCameraManipulator(manip);

    viewer.setSceneData(root);

    std::cout << "[OSGViewer] Scene ready. Running viewer..." << std::endl;
    std::cout << "  Press 'S' -> Stats / FPS" << std::endl;
    std::cout << "  Press 'F' -> Fullscreen" << std::endl;
    std::cout << "  Press 'Esc' -> Quit" << std::endl;

    return viewer.run();
}