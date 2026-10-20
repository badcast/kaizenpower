#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/Types.hpp"
#include "../core/ProfileManager.hpp"
#include <memory>
#include <vector>

namespace fc {

class ProfilesView {
public:
    ProfilesView(std::shared_ptr<ProfileManager> profileMgr);
    ~ProfilesView() = default;

    GtkWidget* getWidget() const { return m_rootBox; }
    void updateActiveState(SystemProfile activeProfile);
    void setOnProfileAppliedCallback(std::function<void(SystemProfile)> cb) {
        m_onApplied = cb;
    }

private:
    std::shared_ptr<ProfileManager> m_profileMgr;
    std::function<void(SystemProfile)> m_onApplied;

    GtkWidget* m_rootBox = nullptr;

    struct ProfileCardItem {
        SystemProfile profile;
        GtkWidget* cardBox;
        GtkWidget* activeBadge;
        GtkWidget* applyBtn;
    };
    std::vector<ProfileCardItem> m_cards;

    void buildUI();
    void createProfileCard(SystemProfile profile, const std::string& iconName);
    static void onProfileApplyClicked(GtkButton* btn, gpointer userData);
};

} // namespace fc
