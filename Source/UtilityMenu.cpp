#include "UtilityMenu.h"
#include "juce_cryptography/juce_cryptography.h"

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
// Session file path — standard JUCE app data location
//   Linux:   ~/.config/QwikRef/session.json
//   macOS:   ~/Library/Application Support/QwikRef/session.json
//   Windows: %APPDATA%/QwikRef/session.json
//==============================================================================
juce::File UtilityMenu::getSessionFile() {
    const auto appDataDir = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
            .getChildFile(JucePlugin_Name);
    auto dir = appDataDir.createDirectory();
    return appDataDir.getChildFile("session.json");
}

bool UtilityMenu::removeSessionFile() {
    const auto file = getSessionFile();
    return file.deleteFile();
}

//==============================================================================
// Token encryption / decryption using BlowFish + hardware fingerprint
//==============================================================================
juce::String UtilityMenu::encryptToken(const juce::String &token) {
    auto key = generateHardwareFingerprint();
    juce::BlowFish blowfish(key.toRawUTF8(), static_cast<int>(key.getNumBytesAsUTF8()));

    juce::MemoryBlock data(token.toRawUTF8(), token.getNumBytesAsUTF8());
    blowfish.encrypt(data);

    return juce::Base64::toBase64(data.getData(), data.getSize());
}

juce::String UtilityMenu::decryptToken(const juce::String &encryptedToken) {
    auto key = generateHardwareFingerprint();
    juce::BlowFish blowfish(key.toRawUTF8(), static_cast<int>(key.getNumBytesAsUTF8()));

    juce::MemoryBlock data;
    juce::Base64::convertFromBase64(data, encryptedToken);
    blowfish.decrypt(data);

    return juce::String(static_cast<const char *>(data.getData()), data.getSize());
}

//==============================================================================
// Session persistence
//==============================================================================
bool UtilityMenu::loadSession(SessionData &data) {
    const auto file = getSessionFile();
    if (!file.existsAsFile())
        return false;

    const auto jsonText = file.loadFileAsString();
    const auto parsed = juce::JSON::parse(jsonText);

    if (auto *obj = parsed.getDynamicObject()) {
        data.encryptedToken = obj->getProperty("token").toString();
        data.email = obj->getProperty("email").toString();
        data.lastAuthorizedDate = obj->getProperty("lastAuthorizedDate").toString();
        return data.encryptedToken.isNotEmpty();
    }
    return false;
}

void UtilityMenu::saveSession(const SessionData &data) {
    auto *obj = new juce::DynamicObject();
    obj->setProperty("token", data.encryptedToken);
    obj->setProperty("email", data.email);
    obj->setProperty("lastAuthorizedDate", data.lastAuthorizedDate);

    auto file = getSessionFile();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

//==============================================================================
// Auth flow — decrypt stored token → auth-refresh → offline grace period
//==============================================================================
void UtilityMenu::startAuthFlow() {
    SessionData session;
    if (!loadSession(session)) {
        // No session file — full login required
        login(false);
        return;
    }

    // Decrypt the stored token using hardware fingerprint
    auto token = decryptToken(session.encryptedToken);
    if (token.isEmpty()) {
        login(false);
        return;
    }

    // Attempt auth-refresh with PocketBase
    juce::URL refreshUrl("http://192.168.4.23:9090/api/collections/users/auth-refresh");

    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders("Content-Type: application/json\r\nAuthorization: Bearer " + token + "\r\n")
            .withHttpRequestCmd("POST")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode);

    auto stream = refreshUrl.createInputStream(options);

    if (stream != nullptr && statusCode >= 200 && statusCode < 300) {
        // Auth refresh succeeded — user is authorized
        juce::String responseBody = stream->readEntireStreamAsString();
        auto parsed = juce::JSON::parse(responseBody);

        if (auto *rootObj = parsed.getDynamicObject()) {
            auto newToken = rootObj->getProperty("token").toString();
            authToken = newToken;
            currentEmail = session.email;

            // Update session file with new token and reset lastAuthorizedDate
            SessionData updatedSession;
            updatedSession.encryptedToken = encryptToken(newToken);
            updatedSession.email = session.email;
            updatedSession.lastAuthorizedDate = juce::Time::getCurrentTime().toISO8601(true);
            saveSession(updatedSession);
        }

        authState.store(authorized);
    } else if (statusCode == 0) {
        // Offline — check last authorized date for 14-day grace period
        auto lastAuth = juce::Time::fromISO8601(session.lastAuthorizedDate);
        auto daysSinceAuth = (juce::Time::getCurrentTime() - lastAuth).inDays();

        if (daysSinceAuth <= 14.0) {
            // Within grace period — allow offline usage
            authToken = token;
            currentEmail = session.email;
            authState.store(authorized);
        } else {
            // Grace period expired — must reconnect
            DBG("Offline grace period expired. Please reconnect to the internet.");
            login(false);
        }
    } else {
        // Auth refresh failed (token expired, etc.) — full login required
        DBG("Auth refresh failed with status: " << statusCode);
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

                    if (auto *recordObj = recordVar.getDynamicObject()) {
                        auto expandVar = recordObj->getProperty("expand");

                        if (auto *expandObj = expandVar.getDynamicObject()) {
                            auto registeredPluginsVar = expandObj->getProperty("registeredPlugins");
                            auto pluginsArray = registeredPluginsVar.getArray();

                            for (auto &plugin: *pluginsArray) {
                                if (auto *pluginObj = plugin.getDynamicObject()) {
                                    if (auto name = pluginObj->getProperty("pluginName").toString();
                                        name == JucePlugin_Name) {
                                        pluginFound = true;
                                        apiPluginKeyCode = pluginObj->getProperty("pluginKeyCode").toString();
                                        apiPluginRecordId = pluginObj->getProperty("id").toString();

                                        // Loop through expanded deviceIDs to find hardware match
                                        auto pluginExpandVar = pluginObj->getProperty("expand");
                                        if (auto *pluginExpandObj = pluginExpandVar.getDynamicObject()) {
                                            auto deviceIDsVar = pluginExpandObj->getProperty("deviceIDs");
                                            if (auto *deviceIDsArray = deviceIDsVar.getArray()) {
                                                auto localFingerprint = generateHardwareFingerprint();
                                                for (auto &device: *deviceIDsArray) {
                                                    if (auto *deviceObj = device.getDynamicObject()) {
                                                        if (auto hwID = deviceObj->getProperty("hardwareID").toString();
                                                            hwID == localFingerprint) {
                                                            hardwareMatched = true;
                                                            apiActivationRecordId = deviceObj->getProperty("id").
                                                                    toString();
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
                            "http://192.168.4.23:9090/api/collections/activations/records/" + apiActivationRecordId);
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

                        // Save encrypted session token
                        SessionData session;
                        session.encryptedToken = encryptToken(authToken);
                        session.email = currentEmail;
                        session.lastAuthorizedDate = juce::Time::getCurrentTime().toISO8601(true);
                        saveSession(session);
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

void UtilityMenu::authorize(const juce::String &errorMessage) {
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
            postObj->setProperty("machine_id", hwFingerprint);
            postObj->setProperty("plugin_id", apiPluginRecordId);
            juce::var postJson(postObj);

            juce::URL postUrl(
                "http://192.168.4.23:9090/api/custom/register-device");
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

            if (postStatusCode >= 200 && postStatusCode < 300) {
                DBG("Activation record created successfully.");

                // Parse activation ID from POST response (nested under "activation")
                auto postParsed = juce::JSON::parse(postResponseBody);
                if (auto *postRespObj = postParsed.getDynamicObject()) {
                    auto activationVar = postRespObj->getProperty("activation");
                    if (auto *activationObj = activationVar.getDynamicObject())
                        apiActivationRecordId = activationObj->getProperty("id").toString();
                }

                // Save encrypted session token
                SessionData session;
                session.encryptedToken = encryptToken(authToken);
                session.email = currentEmail;
                session.lastAuthorizedDate = juce::Time::getCurrentTime().toISO8601(true);
                saveSession(session);

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
        } else if (result == 4) {
            // logout pressed
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
    SessionData session;
    if (!loadSession(session)) {
        DBG("No session found. Redirecting to login.");
        login(false);
        return;
    }

    juce::String statusMsg;
    statusMsg << "Registered Email: " << session.email << "\n\n";
    statusMsg << "Last Authorized: " << session.lastAuthorizedDate;

    asyncAlertWindow = std::make_unique<SafeAlertWindow>(
        "Session Status", "", juce::MessageBoxIconType::InfoIcon);
    asyncAlertWindow->addTextBlock(statusMsg);

    asyncAlertWindow->addButton("logout", 3, {});
    asyncAlertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    asyncAlertWindow->setAlwaysOnTop(true);

    const auto callback = juce::ModalCallbackFunction::create([this](int result) {
        if (asyncAlertWindow != nullptr) {
            asyncAlertWindow->exitModalState(result);
            asyncAlertWindow->setVisible(false);
        }

        if (result == 3) // logout button
            logout();
    });

    asyncAlertWindow->enterModalState(true, callback, false);
}

void UtilityMenu::logout() {

    juce::URL deleteUrl(
                "http://192.168.4.23:9090/api/collections/activations/records/" + apiActivationRecordId);

    int deleteStatusCode = 0;
    auto deleteOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders(
                "Content-Type: application/json\r\nAuthorization: Bearer " + authToken + "\r\n")
            .withHttpRequestCmd("DELETE")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&deleteStatusCode);

    auto deleteStream = deleteUrl.createInputStream(deleteOptions);

    if (deleteStatusCode != 204 && removeSessionFile()) {
        authState.store(loggedOut);
        currentEmail = {};
        apiPluginKeyCode = {};
        apiPluginRecordId = {};
        authToken = {};
    }
}
