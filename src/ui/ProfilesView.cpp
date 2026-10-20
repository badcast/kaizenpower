#include "ProfilesView.hpp"
#include "UIUtils.hpp"
#include "../core/I18n.hpp"

namespace fc {

ProfilesView::ProfilesView(std::shared_ptr<ProfileManager> profileMgr)
    : m_profileMgr(std::move(profileMgr)) {
    buildUI();
}

void ProfilesView::createProfileCard(SystemProfile profile, const std::string& iconName) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_add_css_class(card, "profile-card");

    GtkWidget* topRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(card), topRow);

    GtkWidget* icon = gtk_image_new_from_icon_name(iconName.c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(topRow), icon);

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_hexpand(titleBox, TRUE);
    gtk_box_append(GTK_BOX(topRow), titleBox);

    std::string title = m_profileMgr->getProfileName(profile);
    GtkWidget* titleLbl = gtk_label_new(title.c_str());
    gtk_widget_set_halign(titleLbl, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(titleLbl), ("<span font_weight='bold'>" + title + "</span>").c_str());
    gtk_box_append(GTK_BOX(titleBox), titleLbl);

    std::string desc = m_profileMgr->getProfileDescription(profile);
    GtkWidget* descLbl = gtk_label_new(desc.c_str());
    gtk_widget_set_halign(descLbl, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(descLbl), TRUE);
    gtk_widget_add_css_class(descLbl, "dim-label");
    gtk_box_append(GTK_BOX(titleBox), descLbl);

    // Active Badge
    GtkWidget* activeBadge = UIUtils::createBadge(_("ACTIVE"), "fan-badge-case");
    gtk_box_append(GTK_BOX(topRow), activeBadge);

    // Action button
    GtkWidget* applyBtn = gtk_button_new_with_label(_("Activate"));
    gtk_widget_add_css_class(applyBtn, "pill-btn");
    gtk_widget_add_css_class(applyBtn, "suggested-action");
    gtk_widget_set_valign(applyBtn, GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(applyBtn), "profile_type", GINT_TO_POINTER(static_cast<int>(profile)));
    g_signal_connect(applyBtn, "clicked", G_CALLBACK(onProfileApplyClicked), this);
    gtk_box_append(GTK_BOX(topRow), applyBtn);

    gtk_box_append(GTK_BOX(m_rootBox), card);

    ProfileCardItem item;
    item.profile = profile;
    item.cardBox = card;
    item.activeBadge = activeBadge;
    item.applyBtn = applyBtn;
    m_cards.push_back(item);
}

void ProfilesView::buildUI() {
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(m_rootBox, 10);
    gtk_widget_set_margin_end(m_rootBox, 10);
    gtk_widget_set_margin_top(m_rootBox, 6);
    gtk_widget_set_margin_bottom(m_rootBox, 6);

    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(m_rootBox), headerBox);

    GtkWidget* title = gtk_label_new(_("System Profiles"));
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(title), ("<span size='large' weight='bold'>" + std::string(_("System Profiles")) + "</span>").c_str());
    gtk_box_append(GTK_BOX(headerBox), title);

    GtkWidget* subtitle = gtk_label_new(_("One-click switching between CPU, GPU, and fan cooling behavior."));
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(subtitle, "dim-label");
    gtk_box_append(GTK_BOX(headerBox), subtitle);

    createProfileCard(SystemProfile::Silent, "audio-volume-muted-symbolic");
    createProfileCard(SystemProfile::Balanced, "preferences-system-symbolic");
    createProfileCard(SystemProfile::Performance, "speedometer-symbolic");
    createProfileCard(SystemProfile::MaxCooling, "view-refresh-symbolic");

    updateActiveState(m_profileMgr->getCurrentProfile());
}

void ProfilesView::updateActiveState(SystemProfile activeProfile) {
    for (auto& item : m_cards) {
        bool isActive = (item.profile == activeProfile);
        if (isActive) {
            gtk_widget_add_css_class(item.cardBox, "active-profile");
            gtk_widget_set_visible(item.activeBadge, TRUE);
            gtk_widget_set_sensitive(item.applyBtn, FALSE);
        } else {
            gtk_widget_remove_css_class(item.cardBox, "active-profile");
            gtk_widget_set_visible(item.activeBadge, FALSE);
            gtk_widget_set_sensitive(item.applyBtn, TRUE);
        }
    }
}

void ProfilesView::onProfileApplyClicked(GtkButton* btn, gpointer userData) {
    ProfilesView* self = static_cast<ProfilesView*>(userData);
    for (const auto& item : self->m_cards) {
        if (item.applyBtn == GTK_WIDGET(btn)) {
            SystemProfile targetProfile = item.profile;
            if (self->m_profileMgr) {
                self->m_profileMgr->applyProfile(targetProfile);
                self->updateActiveState(targetProfile);
                if (self->m_onApplied) {
                    self->m_onApplied(targetProfile);
                }
            }
            return;
        }
    }
}

} // namespace fc
