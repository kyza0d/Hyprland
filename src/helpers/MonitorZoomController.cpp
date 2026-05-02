#include "MonitorZoomController.hpp"

#include <algorithm>
#include <cmath>
#include <hyprlang.hpp>
#include "../config/ConfigValue.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/OpenGL.hpp"
#include "Monitor.hpp"
#include "render/Renderer.hpp"

static constexpr float ZOOM_LEVEL_EPSILON = 0.001F;

static CBox            clampedZoomSource(PHLMONITOR pMonitor, const Vector2D& anchorMonitorLocal, float zoom) {
    const Vector2D SIZE = pMonitor->m_size / zoom;
    return {std::clamp(anchorMonitorLocal.x - SIZE.x / 2.0, 0.0, pMonitor->m_size.x - SIZE.x), std::clamp(anchorMonitorLocal.y - SIZE.y / 2.0, 0.0, pMonitor->m_size.y - SIZE.y),
            SIZE.x, SIZE.y};
}

static void clampZoomSourceToMonitor(CBox& source, PHLMONITOR pMonitor) {
    source.w = std::min(source.w, pMonitor->m_size.x);
    source.h = std::min(source.h, pMonitor->m_size.y);
    source.x = std::clamp(source.x, 0.0, pMonitor->m_size.x - source.w);
    source.y = std::clamp(source.y, 0.0, pMonitor->m_size.y - source.h);
}

CBox CMonitorZoomController::zoomSourceWithDetachedCamera(PHLMONITOR pMonitor, float zoom) {
    const auto MOUSE = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    if (m_resetCameraState || std::abs(m_lastZoomLevel - zoom) > ZOOM_LEVEL_EPSILON) {
        if (m_resetCameraState) {
            m_resetCameraState = false;
            m_camera           = clampedZoomSource(pMonitor, MOUSE, zoom);
            m_lastZoomLevel    = zoom;
            return m_camera;
        }

        const auto CENTER = m_camera.pos() + m_camera.size() / 2.0;
        const auto SIZE   = pMonitor->m_size / zoom;
        m_camera          = CBox{CENTER - SIZE / 2.0, SIZE};
        clampZoomSourceToMonitor(m_camera, pMonitor);

        // Resizing the viewport should not re-anchor it to the cursor. If the resize leaves the cursor outside
        // the boundary, push the viewport only as much as needed below.
        m_lastZoomLevel = zoom;
    }

    // Dead-zone camera: the zoom source is fixed while the cursor moves inside it, and is pushed only at its edges.
    if (!m_camera.containsPoint(MOUSE)) {
        if (MOUSE.x < m_camera.x)
            m_camera.x = MOUSE.x;
        if (MOUSE.y < m_camera.y)
            m_camera.y = MOUSE.y;
        if (MOUSE.y > m_camera.y + m_camera.h)
            m_camera.y = MOUSE.y - m_camera.h;
        if (MOUSE.x > m_camera.x + m_camera.w)
            m_camera.x = MOUSE.x - m_camera.w;
    }

    clampZoomSourceToMonitor(m_camera, pMonitor);

    return m_camera;
}

CBox CMonitorZoomController::zoomSource(PHLMONITOR pMonitor, float zoom, bool useMouse, bool forceDetached) {
    static auto PZOOMDETACHEDCAMERA = CConfigValue<Hyprlang::INT>("cursor:zoom_detached_camera");

    if (!pMonitor || zoom <= 1.0F) {
        m_resetCameraState = true;
        return {};
    }

    const auto INITANIM = pMonitor->m_zoomAnimProgress->value() != 1.0;

    if ((*PZOOMDETACHEDCAMERA || forceDetached) && useMouse && !INITANIM)
        return zoomSourceWithDetachedCamera(pMonitor, zoom);

    m_resetCameraState = true;

    const auto ANCHOR = useMouse ? g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position : pMonitor->m_size / 2.0;
    return clampedZoomSource(pMonitor, ANCHOR, zoom);
}

void CMonitorZoomController::applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData) {
    static auto PZOOMRIGID          = CConfigValue<Hyprlang::INT>("cursor:zoom_rigid");
    static auto PZOOMDETACHEDCAMERA = CConfigValue<Hyprlang::INT>("cursor:zoom_detached_camera");
    const auto  ZOOM                = m_renderData.mouseZoomFactor;

    if (ZOOM == 1.0f)
        return;

    const auto m        = m_renderData.pMonitor.lock();
    if (!m)
        return;

    const auto ORIGINAL = monbox;
    const auto INITANIM = m->m_zoomAnimProgress->value() != 1.0;

    if (*PZOOMDETACHEDCAMERA && !INITANIM) {
        const auto SOURCE = zoomSourceWithDetachedCamera(m, ZOOM);
        const auto SCALE  = ZOOM * m->m_scale;
        monbox            = CBox(0, 0, m->m_size.x, m->m_size.y).scale(SCALE).translate(-SOURCE.pos() * SCALE);
    } else {
        const auto ZOOMCENTER = m_renderData.mouseZoomUseMouse ? (g_pInputManager->getMouseCoordsInternal() - m->m_position) * m->m_scale : m->m_transformedSize / 2.f;

        monbox.translate(-ZOOMCENTER).scale(ZOOM).translate(*PZOOMRIGID ? m->m_transformedSize / 2.0 : ZOOMCENTER);
    }

    monbox.x = std::min(monbox.x, 0.0);
    monbox.y = std::min(monbox.y, 0.0);
    if (monbox.x + monbox.width < ORIGINAL.w)
        monbox.x = ORIGINAL.w - monbox.width;
    if (monbox.y + monbox.height < ORIGINAL.h)
        monbox.y = ORIGINAL.h - monbox.height;
}
