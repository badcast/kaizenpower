#include "FanCurveWidget.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace fc {

static const double ML = 28.0; // Margin left
static const double MR = 12.0; // Margin right
static const double MT = 12.0; // Margin top
static const double MB = 18.0; // Margin bottom

static inline double coordToX(double temp, double gw) {
    return ML + (std::clamp(temp, 0.0, 100.0) / 100.0) * gw;
}

static inline double coordToY(double speed, double gh) {
    return MT + gh - (std::clamp(speed, 0.0, 100.0) / 100.0) * gh;
}

static inline double xToTemp(double x, double gw) {
    return ((x - ML) / gw) * 100.0;
}

static inline double yToSpeed(double y, double gh) {
    return ((MT + gh - y) / gh) * 100.0;
}

FanCurveWidget::FanCurveWidget() {
    m_points = {
        {30, 25},
        {50, 45},
        {70, 75},
        {85, 100}
    };

    m_drawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(m_drawingArea, 220, 75);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_drawingArea), onDraw, this, nullptr);

    // Mouse drag gesture
    GtkGesture* drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(onDragBegin), this);
    g_signal_connect(drag, "drag-update", G_CALLBACK(onDragUpdate), this);
    g_signal_connect(drag, "drag-end", G_CALLBACK(onDragEnd), this);
    gtk_widget_add_controller(m_drawingArea, GTK_EVENT_CONTROLLER(drag));

    // Mouse motion controller for hover state
    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(onMotion), this);
    g_signal_connect(motion, "leave", G_CALLBACK(onLeave), this);
    gtk_widget_add_controller(m_drawingArea, motion);
}

void FanCurveWidget::setPoints(const std::vector<CurvePoint>& points) {
    m_points = points;
    std::sort(m_points.begin(), m_points.end(), [](const CurvePoint& a, const CurvePoint& b) {
        return a.tempC < b.tempC;
    });
    gtk_widget_queue_draw(m_drawingArea);
}

void FanCurveWidget::setCurrentTemp(double tempC) {
    if (std::abs(m_currentTemp - tempC) >= 0.5) {
        m_currentTemp = tempC;
        gtk_widget_queue_draw(m_drawingArea);
    }
}

void FanCurveWidget::onDragBegin(GtkGestureDrag*, double start_x, double start_y, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);
    int width = gtk_widget_get_width(self->m_drawingArea);
    int height = gtk_widget_get_height(self->m_drawingArea);
    double gw = width - ML - MR;
    double gh = height - MT - MB;
    if (gw <= 10.0 || gh <= 10.0) return;

    int bestIdx = -1;
    double bestDistSq = 16.0 * 16.0; // 16px hit radius

    for (size_t i = 0; i < self->m_points.size(); ++i) {
        double px = coordToX(self->m_points[i].tempC, gw);
        double py = coordToY(self->m_points[i].speedPercent, gh);
        double dx = start_x - px;
        double dy = start_y - py;
        double distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = static_cast<int>(i);
        }
    }

    self->m_draggedIdx = bestIdx;
    if (bestIdx >= 0) {
        gtk_widget_queue_draw(self->m_drawingArea);
    }
}

void FanCurveWidget::onDragUpdate(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);
    if (self->m_draggedIdx < 0 || self->m_draggedIdx >= static_cast<int>(self->m_points.size())) return;

    int width = gtk_widget_get_width(self->m_drawingArea);
    int height = gtk_widget_get_height(self->m_drawingArea);
    double gw = width - ML - MR;
    double gh = height - MT - MB;
    if (gw <= 10.0 || gh <= 10.0) return;

    double start_x, start_y;
    gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);

    double cur_x = start_x + offset_x;
    double cur_y = start_y + offset_y;

    double rawTemp = xToTemp(cur_x, gw);
    double rawSpeed = yToSpeed(cur_y, gh);

    int idx = self->m_draggedIdx;
    double minT = (idx > 0) ? (self->m_points[idx - 1].tempC + 2) : 10;
    double maxT = (idx + 1 < static_cast<int>(self->m_points.size())) ? (self->m_points[idx + 1].tempC - 2) : 100;

    int finalTemp = static_cast<int>(std::round(std::clamp(rawTemp, minT, maxT)));
    int finalSpeed = static_cast<int>(std::round(std::clamp(rawSpeed, 0.0, 100.0)));

    self->m_points[idx].tempC = finalTemp;
    self->m_points[idx].speedPercent = finalSpeed;

    gtk_widget_queue_draw(self->m_drawingArea);
    if (self->m_onPointsChanged) {
        self->m_onPointsChanged(self->m_points);
    }
}

void FanCurveWidget::onDragEnd(GtkGestureDrag*, double, double, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);
    self->m_draggedIdx = -1;
    gtk_widget_queue_draw(self->m_drawingArea);
    if (self->m_onPointsChanged) {
        self->m_onPointsChanged(self->m_points);
    }
}

void FanCurveWidget::onMotion(GtkEventControllerMotion*, double x, double y, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);
    if (self->m_draggedIdx >= 0) return;

    int width = gtk_widget_get_width(self->m_drawingArea);
    int height = gtk_widget_get_height(self->m_drawingArea);
    double gw = width - ML - MR;
    double gh = height - MT - MB;
    if (gw <= 10.0 || gh <= 10.0) return;

    int bestIdx = -1;
    double bestDistSq = 14.0 * 14.0;

    for (size_t i = 0; i < self->m_points.size(); ++i) {
        double px = coordToX(self->m_points[i].tempC, gw);
        double py = coordToY(self->m_points[i].speedPercent, gh);
        double dx = x - px;
        double dy = y - py;
        double distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestIdx != self->m_hoveredIdx) {
        self->m_hoveredIdx = bestIdx;
        gtk_widget_queue_draw(self->m_drawingArea);
    }
}

void FanCurveWidget::onLeave(GtkEventControllerMotion*, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);
    if (self->m_hoveredIdx != -1) {
        self->m_hoveredIdx = -1;
        gtk_widget_queue_draw(self->m_drawingArea);
    }
}

void FanCurveWidget::onDraw(GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer userData) {
    FanCurveWidget* self = static_cast<FanCurveWidget*>(userData);

    double gw = width - ML - MR;
    double gh = height - MT - MB;
    if (gw <= 10.0 || gh <= 10.0) return;

    // 1. Background box
    cairo_set_source_rgba(cr, 0.11, 0.12, 0.14, 0.90);
    cairo_rectangle(cr, ML, MT, gw, gh);
    cairo_fill(cr);

    // 2. Grid lines
    cairo_set_line_width(cr, 0.7);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.07);

    // Horizontal grid (25%, 50%, 75%, 100%)
    for (int p : {25, 50, 75, 100}) {
        double y = coordToY(p, gh);
        cairo_move_to(cr, ML, y);
        cairo_line_to(cr, ML + gw, y);
    }
    // Vertical grid (25°C, 50°C, 75°C)
    for (int t : {25, 50, 75}) {
        double x = coordToX(t, gw);
        cairo_move_to(cr, x, MT);
        cairo_line_to(cr, x, MT + gh);
    }
    cairo_stroke(cr);

    if (self->m_points.empty()) return;

    // 3. Curve fill under line
    cairo_pattern_t* pat = cairo_pattern_create_linear(0, MT, 0, MT + gh);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.21, 0.52, 0.89, 0.35); // Accent blue
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.21, 0.52, 0.89, 0.03);

    cairo_move_to(cr, coordToX(0, gw), coordToY(self->m_points.front().speedPercent, gh));
    for (const auto& pt : self->m_points) {
        cairo_line_to(cr, coordToX(pt.tempC, gw), coordToY(pt.speedPercent, gh));
    }
    cairo_line_to(cr, coordToX(100, gw), coordToY(self->m_points.back().speedPercent, gh));
    cairo_line_to(cr, coordToX(100, gw), MT + gh);
    cairo_line_to(cr, coordToX(0, gw), MT + gh);
    cairo_close_path(cr);

    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    // 4. Curve stroke
    cairo_set_source_rgba(cr, 0.21, 0.52, 0.89, 1.0); // #3584e4
    cairo_set_line_width(cr, 1.8);
    cairo_move_to(cr, coordToX(0, gw), coordToY(self->m_points.front().speedPercent, gh));
    for (const auto& pt : self->m_points) {
        cairo_line_to(cr, coordToX(pt.tempC, gw), coordToY(pt.speedPercent, gh));
    }
    cairo_line_to(cr, coordToX(100, gw), coordToY(self->m_points.back().speedPercent, gh));
    cairo_stroke(cr);

    // 5. Control Points
    for (size_t i = 0; i < self->m_points.size(); ++i) {
        const auto& pt = self->m_points[i];
        double px = coordToX(pt.tempC, gw);
        double py = coordToY(pt.speedPercent, gh);

        bool isDragged = (static_cast<int>(i) == self->m_draggedIdx);
        bool isHovered = (static_cast<int>(i) == self->m_hoveredIdx);

        double radius = isDragged ? 5.5 : (isHovered ? 4.8 : 3.5);

        // Outer glow on hover or drag
        if (isDragged || isHovered) {
            cairo_arc(cr, px, py, radius + 3.0, 0, 2 * M_PI);
            cairo_set_source_rgba(cr, 0.21, 0.52, 0.89, 0.35);
            cairo_fill(cr);
        }

        cairo_arc(cr, px, py, radius, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.11, 0.44, 0.85, 1.0);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        // Tooltip bubble for dragged point
        if (isDragged) {
            std::string text = std::to_string(pt.tempC) + "°C, " + std::to_string(pt.speedPercent) + "%";
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 9.0);

            cairo_text_extents_t ext;
            cairo_text_extents(cr, text.c_str(), &ext);

            double bubbleW = ext.width + 10.0;
            double bubbleH = ext.height + 6.0;
            double bubbleX = std::clamp(px - bubbleW / 2.0, ML, ML + gw - bubbleW);
            double bubbleY = py - radius - bubbleH - 4.0;
            if (bubbleY < MT) bubbleY = py + radius + 4.0;

            // Bubble background
            cairo_set_source_rgba(cr, 0.15, 0.17, 0.20, 0.95);
            cairo_rectangle(cr, bubbleX, bubbleY, bubbleW, bubbleH);
            cairo_fill(cr);

            cairo_set_source_rgba(cr, 0.21, 0.52, 0.89, 0.8);
            cairo_set_line_width(cr, 1.0);
            cairo_rectangle(cr, bubbleX, bubbleY, bubbleW, bubbleH);
            cairo_stroke(cr);

            // Bubble text
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_move_to(cr, bubbleX + 5.0, bubbleY + bubbleH - 4.0);
            cairo_show_text(cr, text.c_str());
        }
    }

    // 6. Current Temperature cursor
    if (self->m_currentTemp > 0.0) {
        double curX = coordToX(self->m_currentTemp, gw);
        cairo_set_line_width(cr, 1.0);
        const double dashes[] = {3.0, 2.0};
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_source_rgba(cr, 0.98, 0.45, 0.15, 0.85); // Orange marker

        cairo_move_to(cr, curX, MT);
        cairo_line_to(cr, curX, MT + gh);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);

        // Find intersection speed
        FanInfo tempFan;
        tempFan.curvePoints = self->m_points;
        int targetSpeed = tempFan.evaluateCurve(self->m_currentTemp);
        double curY = coordToY(targetSpeed, gh);

        cairo_arc(cr, curX, curY, 3.8, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 1.0, 0.45, 0.15, 1.0);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    // 7. Axis text
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.45);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 8.5);

    // X Axis (0°C, 50°C, 100°C)
    cairo_move_to(cr, ML - 3, height - 3);
    cairo_show_text(cr, "0°C");

    cairo_move_to(cr, coordToX(50, gw) - 9, height - 3);
    cairo_show_text(cr, "50°C");

    cairo_move_to(cr, ML + gw - 22, height - 3);
    cairo_show_text(cr, "100°C");

    // Y Axis (100%, 0%)
    cairo_move_to(cr, 2, coordToY(100, gh) + 7);
    cairo_show_text(cr, "100%");

    cairo_move_to(cr, 4, coordToY(0, gh) - 2);
    cairo_show_text(cr, "0%");
}

} // namespace fc
