#pragma once
// SystemMonitor.h
// Do FPS (chrono), RAM (psapi), CPU (GetProcessTimes), GPU time (OSG stats)
// Windows only

#include <osg/NodeCallback>
#include <osgViewer/Viewer>
#include <osgText/Text>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <chrono>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>

// -------------------------------------------------------
// SystemMonitor: cap nhat metrics moi frame
// -------------------------------------------------------
class SystemMonitor
{
public:
    struct Metrics
    {
        double fps        = 0.0;
        double frameMs    = 0.0;   // ms/frame
        double ramMB      = 0.0;   // Working Set MB
        double cpuPercent = 0.0;   // CPU usage cua process
        double gpuMs      = 0.0;   // GPU draw time (tu OSG stats, neu co)
        bool   gpuAvail   = false;
    };

    SystemMonitor()
    {
        // Lay handle process de do CPU/RAM
        m_procHandle  = GetCurrentProcess();
        m_lastCpuTime = _getProcessCpuTime();
        m_lastWallTime = std::chrono::high_resolution_clock::now();
    }

    // Goi moi frame, truyen vao viewer de lay OSG stats
    void update(osgViewer::Viewer* viewer)
    {
        _updateFPS();
        _updateRAM();
        _updateCPU();
        _updateGPU(viewer);
    }

    const Metrics& getMetrics() const { return m_metrics; }

    // Tao string hien thi gon gàng
    std::string format() const
    {
        const Metrics& m = m_metrics;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);

        oss << "FPS: "  << m.fps
            << "  |  Frame: " << m.frameMs << " ms"
            << "  |  RAM: "  << m.ramMB   << " MB"
            << "  |  CPU: "  << m.cpuPercent << " %";

        if (m.gpuAvail)
            oss << "  |  GPU: " << m.gpuMs << " ms";

        return oss.str();
    }

private:
    // --- FPS: average tren 60 frames ---
    void _updateFPS()
    {
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;

        if (dt <= 0.0) return;

        m_frameTimes.push_back(dt);
        if (m_frameTimes.size() > 60)
            m_frameTimes.pop_front();

        double avg = 0.0;
        for (double t : m_frameTimes) avg += t;
        avg /= m_frameTimes.size();

        m_metrics.fps     = 1.0 / avg;
        m_metrics.frameMs = avg * 1000.0;
    }

    // --- RAM: Working Set cua process ---
    void _updateRAM()
    {
        PROCESS_MEMORY_COUNTERS pmc = {};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(m_procHandle, &pmc, sizeof(pmc)))
            m_metrics.ramMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
    }

    // --- CPU: delta ProcessTime / delta WallTime ---
    void _updateCPU()
    {
        auto nowWall  = std::chrono::high_resolution_clock::now();
        ULONGLONG nowCpu = _getProcessCpuTime();

        double wallDt = std::chrono::duration<double>(
            nowWall - m_lastWallTime).count();

        if (wallDt >= 0.1) // cap nhat moi 100ms
        {
            ULONGLONG cpuDt = nowCpu - m_lastCpuTime;
            // cpuDt tinh bang 100ns units
            double cpuSec = cpuDt * 1e-7;

            SYSTEM_INFO si;
            GetSystemInfo(&si);
            int numCores = si.dwNumberOfProcessors;

            m_metrics.cpuPercent =
                (cpuSec / wallDt / numCores) * 100.0;

            m_lastCpuTime  = nowCpu;
            m_lastWallTime = nowWall;
        }
    }

    // --- GPU: doc tu OSG Stats (Timer Query) ---
    void _updateGPU(osgViewer::Viewer* viewer)
    {
        if (!viewer) return;

        osgViewer::Viewer::Cameras cams;
        viewer->getCameras(cams);
        if (cams.empty()) return;

        osg::Stats* stats = cams[0]->getStats();
        if (!stats)
        {
            m_metrics.gpuAvail = false;
            return;
        }

        int fn = (viewer->getFrameStamp()
               ? viewer->getFrameStamp()->getFrameNumber()
               : 0) - 1;

        double gpuTime = 0.0;
        // OSG ghi "GPU draw time" neu driver ho tro GL timer query
        if (stats->getAttribute(fn, "GPU draw time taken", gpuTime))
        {
            m_metrics.gpuMs    = gpuTime * 1000.0;
            m_metrics.gpuAvail = true;
        }
        else
        {
            m_metrics.gpuAvail = false;
        }
    }

    // Lay tong CPU time (kernel + user) cua process, don vi 100ns
    ULONGLONG _getProcessCpuTime()
    {
        FILETIME cr, ex, kernel, user;
        if (GetProcessTimes(m_procHandle, &cr, &ex, &kernel, &user))
        {
            ULARGE_INTEGER k, u;
            k.LowPart  = kernel.dwLowDateTime;
            k.HighPart = kernel.dwHighDateTime;
            u.LowPart  = user.dwLowDateTime;
            u.HighPart = user.dwHighDateTime;
            return k.QuadPart + u.QuadPart;
        }
        return 0;
    }

    HANDLE       m_procHandle;
    Metrics      m_metrics;

    // FPS
    std::deque<double> m_frameTimes;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime
        = std::chrono::high_resolution_clock::now();

    // CPU
    ULONGLONG    m_lastCpuTime;
    std::chrono::high_resolution_clock::time_point m_lastWallTime;
};

// -------------------------------------------------------
// MonitorCallback: update callback gan vao HUD camera
// -------------------------------------------------------
class MonitorCallback : public osg::NodeCallback
{
public:
    MonitorCallback(osgText::Text* fpsText,
                    osgText::Text* sysText,
                    osgViewer::Viewer* viewer)
        : m_fpsText(fpsText)
        , m_sysText(sysText)
        , m_viewer(viewer)
    {}

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        m_monitor.update(m_viewer);
        const SystemMonitor::Metrics& m = m_monitor.getMetrics();

        // FPS (to, mau xanh)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS: %.1f  (%.1f ms)",
                     m.fps, m.frameMs);
            m_fpsText->setText(buf);

            // Mau theo performance
            if (m.fps >= 55)
                m_fpsText->setColor({.3f, 1.f, .3f, 1.f});   // xanh la
            else if (m.fps >= 30)
                m_fpsText->setColor({1.f, .8f, .2f, 1.f});   // vang
            else
                m_fpsText->setColor({1.f, .3f, .3f, 1.f});   // do
        }

        // System stats (nho hon)
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            oss << "RAM: " << m.ramMB << " MB"
                << "   CPU: " << m.cpuPercent << " %";
            if (m.gpuAvail)
                oss << "   GPU: " << m.gpuMs << " ms";
            m_sysText->setText(oss.str());
        }

        traverse(node, nv);
    }

private:
    SystemMonitor      m_monitor;
    osgText::Text*     m_fpsText;
    osgText::Text*     m_sysText;
    osgViewer::Viewer* m_viewer;
};
