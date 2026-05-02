#pragma once

#include "./math/Math.hpp"
#include "../desktop/DesktopTypes.hpp"

struct SCurrentRenderData;

class CMonitorZoomController {
  public:
    bool m_resetCameraState = true;

    CBox zoomSource(PHLMONITOR pMonitor, float zoom, bool useMouse, bool forceDetached = false);
    void applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData);

  private:
    CBox  zoomSourceWithDetachedCamera(PHLMONITOR pMonitor, float zoom);

    CBox  m_camera;
    float m_lastZoomLevel = 1.0f;
};
