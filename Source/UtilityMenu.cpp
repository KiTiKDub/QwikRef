#include "UtilityMenu.h"

bool UtilityMenu::LicenseInfo::isValid() const {
    // Check hardware fingerprint matches this machine
    if (hardwareFingerprint != generateHardwareFingerprint())
        return false;

    // Check expiration
    const auto expiry = juce::Time::fromISO8601(expirationDate);
    return juce::Time::getCurrentTime() < expiry;
}

int UtilityMenu::LicenseInfo::daysUntilExpiration() const {
    const auto expiry = juce::Time::fromISO8601(expirationDate);
    const auto now = juce::Time::getCurrentTime();
    const auto diff = expiry - now;
    return static_cast<int>(diff.inDays());
}

void UtilityMenu::paint(juce::Graphics &g) {
    juce::ignoreUnused(g);
}

void UtilityMenu::resized() {
}

void UtilityMenu::mouseDown(const juce::MouseEvent &e) {
    juce::ignoreUnused(e);
}

void UtilityMenu::mouseEnter(const juce::MouseEvent &e) {
    juce::ignoreUnused(e);
}

void UtilityMenu::mouseExit(const juce::MouseEvent &e) {
    juce::ignoreUnused(e);
}

juce::String UtilityMenu::generateHardwareFingerprint() {
    const auto raw = juce::SystemStats::getComputerName()
               + juce::SystemStats::getOperatingSystemName()
               + juce::SystemStats::getLogonName();
    return juce::Base64::toBase64(raw);
}

//==============================================================================
// License file path — standard JUCE app data location
//   Linux:   ~/.config/QwikRef/license.json
//   macOS:   ~/Library/Application Support/QwikRef/license.json
//   Windows: %APPDATA%/QwikRef/license.json
//==============================================================================
juce::File UtilityMenu::getLicenseFile() {
    const auto appDataDir = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
            .getChildFile(JucePlugin_Name);
    auto dir = appDataDir.createDirectory();
    return appDataDir.getChildFile("license.json");
}

bool UtilityMenu::removeLicenseFile() {
    const auto file = getLicenseFile();
    return file.deleteFile();
}

bool UtilityMenu::loadLicense(LicenseInfo &info) {
    const auto file = getLicenseFile();
    if (!file.existsAsFile())
        return false;

    const auto jsonText = file.loadFileAsString();
    const auto parsed = juce::JSON::parse(jsonText);

    if (auto *obj = parsed.getDynamicObject()) {
        info.productKey = obj->getProperty("productKey").toString();
        info.email = obj->getProperty("email").toString();
        info.hardwareFingerprint = obj->getProperty("hardwareFingerprint").toString();
        info.activationDate = obj->getProperty("activationDate").toString();
        info.expirationDate = obj->getProperty("expirationDate").toString();
        return info.productKey.isNotEmpty() && info.email.isNotEmpty();
    }
    return false;
}

void UtilityMenu::saveLicense(const LicenseInfo &info) {
    auto *obj = new juce::DynamicObject();
    obj->setProperty("productKey", info.productKey);
    obj->setProperty("email", info.email);
    obj->setProperty("hardwareFingerprint", info.hardwareFingerprint);
    obj->setProperty("activationDate", info.activationDate);
    obj->setProperty("expirationDate", info.expirationDate);

    auto file = getLicenseFile();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

void UtilityMenu::startAuthFlow() {
    //These checks don't account for the user logging out prior. I'll have to either do a db store or store on the system
    if (LicenseInfo info; loadLicense(info) && info.isValid()) {
        // Valid license exists — silently authorize, no popup
        authState.store(authorized);
    } else {
        // No license or expired — full login flow
        login(false);
    }
}

void UtilityMenu::login(bool fromLogOut) {
    asyncAlertWindow = std::make_unique<SafeAlertWindow>("Log-in",
                                                         "",
                                                         juce::MessageBoxIconType::NoIcon);

    asyncAlertWindow->addTextBlock("Please enter your email and password");
    asyncAlertWindow->addTextBlock("Email:");
    asyncAlertWindow->addTextEditor("email", "", "");
    asyncAlertWindow->addTextBlock("Password:");
    asyncAlertWindow->addTextEditor("password", "", "", true); // password masking
    asyncAlertWindow->addButton("log-in", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    asyncAlertWindow->addButton("cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    asyncAlertWindow->setAlwaysOnTop(true);

    const auto callback = juce::ModalCallbackFunction::create([this, fromLogOut](int result) {
        if (asyncAlertWindow == nullptr) return;
        auto &aw = *asyncAlertWindow;

        if (result == 1) // log-in pressed
        {
            auto email = aw.getTextEditorContents("email");
            auto password = aw.getTextEditorContents("password");

            aw.exitModalState(result);
            aw.setVisible(false);

            // Build JSON payload
            auto *dataObj = new juce::DynamicObject();
            dataObj->setProperty("identity", email);
            dataObj->setProperty("password", password);
            juce::var dataJson(dataObj);

            // POST to API
            juce::URL apiUrl("http://192.168.4.23:9090/api/collections/users/auth-with-password");
            apiUrl = apiUrl.withParameter("expand", "registeredPlugins,registeredPlugins.deviceIDs");
            auto postUrl = apiUrl.withPOSTData(juce::JSON::toString(dataJson));

            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders("Content-Type: application/json\r\n")
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(5000)
                    .withStatusCode(&statusCode)
                    .withResponseHeaders(&responseHeaders);

            auto stream = postUrl.createInputStream(options);
            juce::String responseBody = stream->readEntireStreamAsString();

            if (stream != nullptr && statusCode >= 200 && statusCode < 300) {
                // Login success — store email for later use
                currentEmail = email;

                // Parse the response JSON
                auto parsed = juce::JSON::parse(responseBody);
                if (auto *rootObj = parsed.getDynamicObject()) {
                    // Store auth token for PATCH requests
                    authToken = rootObj->getProperty("token").toString();

                    // Drill into record.expand.registeredPlugins
                    auto recordVar = rootObj->getProperty("record");
                    bool pluginFound = false;
                    bool hardwareMatched = false;
                    juce::String matchedActivationId;

                    if (auto *recordObj = recordVar.getDynamicObject()) {
                        auto expandVar = recordObj->getProperty("expand");

                        if (auto *expandObj = expandVar.getDynamicObject()) {
                            auto registeredPluginsVar = expandObj->getProperty("registeredPlugins");
                            auto pluginsArray = registeredPluginsVar.getArray();

                            for (auto &plugin: *pluginsArray) {
                                if (auto *pluginObj = plugin.getDynamicObject()) {
                                    if (auto name = pluginObj->getProperty("pluginName").toString(); name == JucePlugin_Name) {
                                        pluginFound = true;
                                        apiPluginKeyCode = pluginObj->getProperty("pluginKeyCode").toString();
                                        apiPluginRecordId = pluginObj->getProperty("id").toString();

                                        // Loop through expanded deviceIDs to find hardware match
                                        auto pluginExpandVar = pluginObj->getProperty("expand");
                                        if (auto *pluginExpandObj = pluginExpandVar.getDynamicObject()) {
                                            auto deviceIDsVar = pluginExpandObj->getProperty("deviceIDs");
                                            if (auto *deviceIDsArray = deviceIDsVar.getArray()) {
                                                auto localFingerprint = generateHardwareFingerprint();
                                                for (auto &device : *deviceIDsArray) {
                                                    if (auto *deviceObj = device.getDynamicObject()) {
                                                        if (auto hwID = deviceObj->getProperty("hardwareID").toString(); hwID == localFingerprint) {
                                                            hardwareMatched = true;
                                                            matchedActivationId = deviceObj->getProperty("id").toString();
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (!pluginFound) {
                        // User doesn't own this plugin — show purchase required
                        authState.store(unauthorized);
                        purchaseRequired();
                    } else if (hardwareMatched) {
                        // This device is already activated — update lastSeen on server
                        auto localFingerprint = generateHardwareFingerprint();

                        auto *lastSeenObj = new juce::DynamicObject();
                        lastSeenObj->setProperty("lastSeen",
                            juce::Time::getCurrentTime().toISO8601(true));
                        juce::var lastSeenJson(lastSeenObj);

                        juce::URL lastSeenUrl(
                            "http://192.168.4.23:9090/api/collections/activations/records/" + matchedActivationId);
                        auto lastSeenUrlWithData = lastSeenUrl.withPOSTData(juce::JSON::toString(lastSeenJson));

                        int lastSeenStatus = 0;
                        auto lastSeenOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                .withExtraHeaders(
                                    "Content-Type: application/json\r\nAuthorization: Bearer " + authToken + "\r\n")
                                .withHttpRequestCmd("PATCH")
                                .withConnectionTimeoutMs(5000)
                                .withStatusCode(&lastSeenStatus);

                        lastSeenUrlWithData.createInputStream(lastSeenOptions);
                        DBG("lastSeen PATCH status: " << lastSeenStatus);

                        // Refresh local license
                        LicenseInfo info;
                        info.productKey = apiPluginKeyCode;
                        info.email = currentEmail;
                        info.hardwareFingerprint = localFingerprint;
                        info.activationDate = juce::Time::getCurrentTime().toISO8601(true);
                        auto expirationTime = juce::Time::getCurrentTime()
                                              + juce::RelativeTime::days(30);
                        info.expirationDate = expirationTime.toISO8601(true);
                        saveLicense(info);
                        authState.store(authorized);
                        status();
                    } else {
                        // No hardware match — go to authorize
                        authState.store(unauthorized);
                        authorize();
                    }
                }
            } else {
                // Login failed — log error to debug console and re-prompt
                if (statusCode == 401)
                    DBG("Login failed: Invalid email or password.");
                else if (statusCode == 0)
                    DBG("Login failed: Could not reach server.");
                else
                    DBG("Login failed with status: " << statusCode);

                login(false); // Re-prompt login
            }
        } else if (result == 0) {
            aw.exitModalState(result);
            aw.setVisible(false);
        }
    });

    asyncAlertWindow->enterModalState(true, callback, false);
}

void UtilityMenu::authorize(const juce::String& errorMessage) {
    asyncAlertWindow = std::make_unique<SafeAlertWindow>("Authorize",
                                                         "",
                                                         juce::MessageBoxIconType::NoIcon);

    if (errorMessage.isNotEmpty())
        asyncAlertWindow->addTextBlock(errorMessage);

    asyncAlertWindow->addTextBlock("Please enter your product activation key");
    asyncAlertWindow->addTextBlock("Product Key:");
    asyncAlertWindow->addTextEditor("key", "", "");
    // if (auto *keyEditor = asyncAlertWindow->getTextEditor("key"))
    // {
    //     keyEditor->setInputRestrictions(36, uuidCharacterRestriction);
    // }
    asyncAlertWindow->addButton("authorize", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    asyncAlertWindow->addButton("logout", 4, {});
    asyncAlertWindow->addButton("cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    asyncAlertWindow->setAlwaysOnTop(true);

    const auto callback = juce::ModalCallbackFunction::create([this](int result) {
        if (asyncAlertWindow == nullptr) return;
        auto &aw = *asyncAlertWindow;

        if (result == 1) // authorize pressed
        {
            auto productKey = aw.getTextEditorContents("key");

            aw.exitModalState(result);
            aw.setVisible(false);

            if (productKey.isEmpty()) {
                DBG("Authorization failed: Product key cannot be empty.");
                authorize(); // Re-prompt
                return;
            }

            // Verify user-entered key matches the API key
            if (productKey != apiPluginKeyCode) {
                DBG("Authorization failed: Product key does not match.");
                authorize("Product key does not match.");
                return;
            }

            // Key matches — POST to create activation record on server
            auto hwFingerprint = generateHardwareFingerprint();

            auto *postObj = new juce::DynamicObject();
            postObj->setProperty("hardwareID", hwFingerprint);
            postObj->setProperty("lastSeen",
                juce::Time::getCurrentTime().toISO8601(true));
            juce::Array<juce::var> pluginArray;
            pluginArray.add(apiPluginRecordId);
            postObj->setProperty("plugin", pluginArray);
            juce::var postJson(postObj);

            juce::URL postUrl(
                "http://192.168.4.23:9090/api/collections/activations/records");
            auto postUrlWithData = postUrl.withPOSTData(juce::JSON::toString(postJson));

            int postStatusCode = 0;
            auto postOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders(
                        "Content-Type: application/json\r\nAuthorization: Bearer " + authToken + "\r\n")
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(5000)
                    .withStatusCode(&postStatusCode);

            auto postStream = postUrlWithData.createInputStream(postOptions);
            juce::String postResponseBody;
            if (postStream != nullptr)
                postResponseBody = postStream->readEntireStreamAsString();

            if (postStream != nullptr && postStatusCode >= 200 && postStatusCode < 300) {
                DBG("Activation record created successfully.");

                // Parse the new activation record ID from the response
                juce::String newActivationId;
                auto parsedResponse = juce::JSON::parse(postResponseBody);
                if (auto *respObj = parsedResponse.getDynamicObject())
                    newActivationId = respObj->getProperty("id").toString();

                // PATCH registeredPlugins to add new activation ID to deviceIDs and set activated
                if (newActivationId.isNotEmpty()) {
                    auto *patchObj = new juce::DynamicObject();
                    patchObj->setProperty("activated", true);

                    // Use deviceIDs+ to append to the relation array
                    juce::Array<juce::var> deviceIDsAppend;
                    deviceIDsAppend.add(newActivationId);
                    patchObj->setProperty("deviceIDs+", deviceIDsAppend);
                    juce::var patchJson(patchObj);

                    juce::URL patchUrl(
                        "http://192.168.4.23:9090/api/collections/registeredPlugins/records/" + apiPluginRecordId);
                    auto patchUrlWithData = patchUrl.withPOSTData(juce::JSON::toString(patchJson));

                    int patchStatusCode = 0;
                    auto patchOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withExtraHeaders(
                                "Content-Type: application/json\r\nAuthorization: Bearer " + authToken + "\r\n")
                            .withHttpRequestCmd("PATCH")
                            .withConnectionTimeoutMs(5000)
                            .withStatusCode(&patchStatusCode);

                    patchUrlWithData.createInputStream(patchOptions);
                    DBG("registeredPlugins PATCH status: " << patchStatusCode);
                }

                // Save local license
                LicenseInfo info;
                info.productKey = productKey;
                info.email = currentEmail;
                info.hardwareFingerprint = hwFingerprint;
                info.activationDate = juce::Time::getCurrentTime().toISO8601(true);
                auto expirationTime = juce::Time::getCurrentTime()
                                      + juce::RelativeTime::days(30);
                info.expirationDate = expirationTime.toISO8601(true);
                saveLicense(info);

                authState.store(authorized);
                status();
            } else if (postStatusCode == 400) {
                // Server rejected — parse and display the error message
                juce::String serverMsg = "Activation failed.";
                auto parsedErr = juce::JSON::parse(postResponseBody);
                if (auto *errObj = parsedErr.getDynamicObject()) {
                    auto msg = errObj->getProperty("message").toString();
                    if (msg.isNotEmpty())
                        serverMsg = msg;
                }
                DBG("Activation rejected: " << serverMsg);
                authorize(serverMsg);
            } else {
                DBG("POST activation failed with status: " << postStatusCode);
                authorize("Activation failed. Please try again.");
            }
        } else if (result == 0) {
            aw.exitModalState(result);
            aw.setVisible(false);
        } else if (result == 4) { // logout pressed
            aw.exitModalState(result);
            aw.setVisible(false);
            logout();
        }
    });

    asyncAlertWindow->enterModalState(true, callback, false);
}

void UtilityMenu::purchaseRequired() {
    asyncAlertWindow = std::make_unique<SafeAlertWindow>(
        "Purchase Required", "", juce::MessageBoxIconType::WarningIcon);

    asyncAlertWindow->addTextBlock(
        juce::String("Your account does not have a license for ") + JucePlugin_Name + ".\n\n"
        "Please purchase a product key to continue.");
    asyncAlertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    asyncAlertWindow->setAlwaysOnTop(true);

    const auto callback = juce::ModalCallbackFunction::create([this](int result) {
        if (asyncAlertWindow != nullptr) {
            asyncAlertWindow->exitModalState(result);
            asyncAlertWindow->setVisible(false);
        }
    });

    asyncAlertWindow->enterModalState(true, callback, false);
}


void UtilityMenu::status() {
    LicenseInfo info;
    if (!loadLicense(info)) {
        DBG("No license found. Redirecting to login.");
        login(false);
        return;
    }

    int daysLeft = info.daysUntilExpiration();
    juce::String statusMsg;
    statusMsg << "Registration Key: " << info.productKey << "\n\n";
    statusMsg << "Registered Email: " << info.email << "\n\n";

    if (daysLeft > 0)
        statusMsg << "Days Until Expiration: " << juce::String(daysLeft);
    else
        statusMsg << "License EXPIRED. Please renew.";

    asyncAlertWindow = std::make_unique<SafeAlertWindow>(
        "License Status", "", juce::MessageBoxIconType::InfoIcon);
    asyncAlertWindow->addTextBlock(statusMsg);

    if (daysLeft <= 0)
        asyncAlertWindow->addButton("renew", 2, {});

    asyncAlertWindow->addButton("logout", 3, {});
    asyncAlertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    asyncAlertWindow->setAlwaysOnTop(true);

    const auto callback = juce::ModalCallbackFunction::create([this](int result) {
        if (asyncAlertWindow != nullptr) {
            asyncAlertWindow->exitModalState(result);
            asyncAlertWindow->setVisible(false);
        }

        if (result == 2) // renew
            login(false);
        else if (result == 3) // logout button
            logout();
    });

    asyncAlertWindow->enterModalState(true, callback, false);
}

void UtilityMenu::logout() {
    if (removeLicenseFile()) {
        authState.store(loggedOut);

        // Clear stored session credentials
        currentEmail = {};
        apiPluginKeyCode = {};
        apiPluginRecordId = {};
        authToken = {};
    }
}
