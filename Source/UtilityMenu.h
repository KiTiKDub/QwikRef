#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include "juce_core/juce_core.h"

struct SafeAlertWindow : public juce::AlertWindow, public juce::DeletedAtShutdown {
public:
    SafeAlertWindow(const juce::String &title, const juce::String &message,
                    const juce::MessageBoxIconType iconType)
        : AlertWindow(title, message, iconType, nullptr) {
        // setLookAndFeel(&lnf);
        // setColour(juce::AlertWindow::ColourIds::backgroundColourId, juce::colors::softblack);
        // setColour(juce::AlertWindow::ColourIds::outlineColourId, juce::colors::black);
        // setColour(juce::AlertWindow::ColourIds::textColourId, juce::colors::white);
    }

    // ~SafeAlertWindow() override {
    //     // setLookAndFeel(nullptr);
    // }

private:
    // EmLookAndFeel lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SafeAlertWindow)
};

struct UtilityMenu : public juce::Component {
    UtilityMenu() = default;

    // ~UtilityMenu();

    enum AuthState {
        loggedOut = 0,
        unauthorized = 1,
        authorized = 2
    };

    std::atomic<int> authState { loggedOut };

    void paint(juce::Graphics &g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;

    //==============================================================================
    // License data structure
    //==============================================================================
    struct LicenseInfo {
        juce::String productKey;
        juce::String email;
        juce::String hardwareFingerprint;
        juce::String activationDate; // ISO 8601 format
        juce::String expirationDate; // ISO 8601 format

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] int daysUntilExpiration() const;
    };

    //==============================================================================
    // Public API
    //==============================================================================
    static juce::String generateHardwareFingerprint();
    static juce::File getLicenseFile();
    bool removeLicenseFile();

    bool loadLicense(LicenseInfo &info);
    void saveLicense(const LicenseInfo &info);
    void startAuthFlow();
    void logout();

    //==============================================================================
    // Dialog flows (public so editor button can invoke them)
    //==============================================================================
    void login(bool fromLogOut);
    void authorize(const juce::String& errorMessage = {});
    void purchaseRequired();
    void status();

private:
    juce::PopupMenu popupMenu;
    std::unique_ptr<SafeAlertWindow> asyncAlertWindow;
    juce::String currentEmail; // stored between login → authorize flow
    juce::String apiPluginKeyCode; // pluginKeyCode from API expand
    juce::String apiPluginRecordId; // record ID for PATCH
    juce::String authToken; // bearer token from login response
};
