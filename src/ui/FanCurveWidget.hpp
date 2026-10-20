#pragma once

#include <gtk/gtk.h>
#include "../core/Types.hpp"
#include <vector>
#include <functional>

namespace fc {

class FanCurveWidget {
public:
    FanCurveWidget();
    ~FanCurveWidget() = default;

    GtkWidget* getWidget() const { return m_drawingArea; }

    void setPoints(const std::vector<CurvePoint>& points);
    const std::vector<CurvePoint>& getPoints() const { return m_points; }

    void setCurrentTemp(double tempC);
    void setOnPointsChangedCallback(std::function<void(const std::vector<CurvePoint>&)> cb) {
        m_onPointsChanged = std::move(cb);
    }

private:
    GtkWidget* m_drawingArea = nullptr;
    std::vector<CurvePoint> m_points;
    double m_currentTemp = 0.0;
    int m_draggedIdx = -1;
    int m_hoveredIdx = -1;
    std::function<void(const std::vector<CurvePoint>&)> m_onPointsChanged;

    static void onDraw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer userData);
    static void onDragBegin(GtkGestureDrag* gesture, double start_x, double start_y, gpointer userData);
    static void onDragUpdate(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer userData);
    static void onDragEnd(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer userData);
    static void onMotion(GtkEventControllerMotion* controller, double x, double y, gpointer userData);
    static void onLeave(GtkEventControllerMotion* controller, gpointer userData);
};

} // namespace fc
