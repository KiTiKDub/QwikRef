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
    // Session data structure — encrypted token + metadata
    //==============================================================================
    struct SessionData {
        juce::String encryptedToken; // Base64(BlowFish(token))
        juce::String email;
        juce::String lastAuthorizedDate; // ISO 8601 format
    };

    //==============================================================================
    // Public API
    //==============================================================================
    static juce::String generateHardwareFingerprint();
    static juce::File getSessionFile();
    bool removeSessionFile();

    bool loadSession(SessionData &data);
    void saveSession(const SessionData &data);
    void startAuthFlow();
    void logout();

    //==============================================================================
    // Token encryption helpers
    //==============================================================================
    static juce::String encryptToken(const juce::String &token);
    static juce::String decryptToken(const juce::String &encryptedToken);

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
    juce::String apiActivationRecordId;
    juce::String authToken; // bearer token from login response
};
