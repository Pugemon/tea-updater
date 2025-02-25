#include "utils.hpp"

#include <switch.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "current_cfw.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "fs.hpp"
#include "main_frame.hpp"
#include "progress_event.hpp"
#include "reboot_payload.h"
#include "unistd.h"

namespace i18n = brls::i18n;
using namespace i18n::literals;

namespace util {

    bool isArchive(const std::string& path)
    {
        if (std::filesystem::exists(path)) {
            std::ifstream is(path, std::ifstream::binary);
            char zip_signature[4] = {0x50, 0x4B, 0x03, 0x04};  // zip signature header PK\3\4
            char signature[4];
            is.read(signature, 4);
            if (is.good() && std::equal(std::begin(signature), std::end(signature), std::begin(zip_signature), std::end(zip_signature))) {
                return true;
            }
        }
        return false;
    }

    void downloadArchive(const std::string& url, contentType type)
    {
        long status_code;
        downloadArchive(url, type, status_code);
    }

    void downloadArchive(const std::string& url, contentType type, long& status_code)
    {
        fs::createTree(DOWNLOAD_PATH);
        switch (type) {
            case contentType::custom:
                status_code = download::downloadFile(url, CUSTOM_FILENAME, OFF);
                break;
            case contentType::cheats:
                status_code = download::downloadFile(url, CHEATS_FILENAME, OFF);
                break;
            case contentType::fw:
                status_code = download::downloadFile(url, FIRMWARE_FILENAME, OFF);
                break;
            case contentType::app:
                status_code = download::downloadFile(url, APP_FILENAME, OFF);
                break;
            case contentType::bootloaders:
                status_code = download::downloadFile(url, BOOTLOADER_FILENAME, OFF);
                break;
            case contentType::ams_cfw:
                status_code = download::downloadFile(url, AMS_FILENAME, OFF);
                break;
            default:
                break;
        }
        ProgressEvent::instance().setStatusCode(status_code);
    }

    void showDialogBoxInfo(const std::string& text)
    {
        brls::Dialog* dialog;
        dialog = new brls::Dialog(text);
        brls::GenericEvent::Callback callback = [dialog](brls::View* view) {
            dialog->close();
        };
        dialog->addButton("menus/common/ok"_i18n, callback);
        dialog->setCancelable(true);
        dialog->open();
    }

    int showDialogBoxBlocking(const std::string& text, const std::string& opt)
    {
        int result = -1;
        brls::Dialog* dialog = new brls::Dialog(text);
        brls::GenericEvent::Callback callback = [dialog, &result](brls::View* view) {
            result = 0;
            dialog->close();
        };
        dialog->addButton(opt, callback);
        dialog->setCancelable(false);
        dialog->open();
        while (result == -1) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::microseconds(800000));
        return result;
    }

    int showDialogBoxBlocking(const std::string& text, const std::string& opt1, const std::string& opt2)
    {
        int result = -1;
        brls::Dialog* dialog = new brls::Dialog(text);
        brls::GenericEvent::Callback callback1 = [dialog, &result](brls::View* view) {
            result = 0;
            dialog->close();
        };
        brls::GenericEvent::Callback callback2 = [dialog, &result](brls::View* view) {
            result = 1;
            dialog->close();
        };
        dialog->addButton(opt1, callback1);
        dialog->addButton(opt2, callback2);
        dialog->setCancelable(false);
        dialog->open();
        while (result == -1) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::microseconds(800000));
        return result;
    }

    void crashIfNotArchive(contentType type)
    {
        std::string filename;
        switch (type) {
            case contentType::custom:
                filename = CUSTOM_FILENAME;
                break;
            case contentType::cheats:
                filename = CHEATS_FILENAME;
                break;
            case contentType::fw:
                filename = FIRMWARE_FILENAME;
                break;
            case contentType::app:
                filename = APP_FILENAME;
                break;
            case contentType::bootloaders:
                filename = BOOTLOADER_FILENAME;
                break;
            case contentType::ams_cfw:
                filename = AMS_FILENAME;
                break;
            default:
                return;
        }
        if (!isArchive(filename)) {
            brls::Application::crash("menus/utils/not_an_archive"_i18n);
        }
    }

    void extractArchive(contentType type, const std::string& version)
    {
        chdir(ROOT_PATH);
        crashIfNotArchive(type);
        switch (type) {
            case contentType::cheats: {
                std::vector<std::string> titles = extract::getInstalledTitlesNs();
                titles = extract::excludeTitles(CHEATS_EXCLUDE, titles);
                extract::extractCheats(CHEATS_FILENAME, titles, CurrentCfw::running_cfw, version);
                break;
            }
            case contentType::fw:
                fs::removeDir(FIRMWARE_PATH);
                fs::createTree(FIRMWARE_PATH);
                extract::extract(FIRMWARE_FILENAME, FIRMWARE_PATH);
                if (std::filesystem::exists(FIRMWARE_FILENAME)) std::filesystem::remove(FIRMWARE_FILENAME);
                break;
            case contentType::app:
                extract::extract(APP_FILENAME, CONFIG_PATH);
                fs::copyFile(ROMFS_FORWARDER, FORWARDER_PATH);
                break;
            case contentType::custom: {
                int preserveInis = showDialogBoxBlocking("menus/utils/overwrite_inis"_i18n, "menus/common/yes"_i18n, "menus/common/no"_i18n);
                extract::extract(CUSTOM_FILENAME, ROOT_PATH, preserveInis);
                break;
            }
            case contentType::bootloaders: {
                int preserveInis = showDialogBoxBlocking("menus/utils/overwrite_inis"_i18n, "menus/common/yes"_i18n, "menus/common/no"_i18n);
                extract::extract(BOOTLOADER_FILENAME, ROOT_PATH, preserveInis);
                break;
            }
            case contentType::ams_cfw: {
                if (std::filesystem::exists(TEA_DIRECTORY_PATH)) std::filesystem::remove_all(TEA_DIRECTORY_PATH);
                std::filesystem::create_directory(TEA_DIRECTORY_PATH);
                extract::extract(CFW_FILENAME, TEA_DIRECTORY_PATH, 1);

                if (std::filesystem::exists("/tea/bootloader/hekate_ipl.ini")) {
                    fs::copyFile("/tea/bootloader/hekate_ipl.ini", "/bootloader/hekate_ipl.ini");
                    fs::copyFile("/tea/config/tea-updater/tea_updater.ini", "/bootloader/ini/!tea_updater.ini");
                    fs::copyFile("/tea/bootloader/res/tu.bmp", "/bootloader/res/tu.bmp");
                    if (std::filesystem::exists(CFW_FILENAME)) std::filesystem::remove_all(CFW_FILENAME);
                }
                break;
            }
            default:
                break;
        }
        if (type == contentType::ams_cfw || type == contentType::bootloaders || type == contentType::custom)
            fs::copyFiles(COPY_FILES_TXT);
    }

    std::string formatListItemTitle(const std::string& str, size_t maxScore)
    {
        size_t score = 0;
        for (size_t i = 0; i < str.length(); i++) {
            score += std::isupper(str[i]) ? 4 : 3;
            if (score > maxScore) {
                return str.substr(0, i - 1) + "\u2026";
            }
        }
        return str;
    }

    std::string formatApplicationId(u64 ApplicationId)
    {
        return fmt::format("{:016X}", ApplicationId);
    }

    std::vector<std::string> fetchPayloads()
    {
        std::vector<std::string> payloadPaths;
        payloadPaths.push_back(ROOT_PATH);
        if (std::filesystem::exists(PAYLOAD_PATH)) payloadPaths.push_back(PAYLOAD_PATH);
        if (std::filesystem::exists(AMS_PATH)) payloadPaths.push_back(AMS_PATH);
        if (std::filesystem::exists(BOOTLOADER_PATH)) payloadPaths.push_back(BOOTLOADER_PATH);
        if (std::filesystem::exists(BOOTLOADER_PL_PATH)) payloadPaths.push_back(BOOTLOADER_PL_PATH);
        if (std::filesystem::exists(ROMFS_PATH)) payloadPaths.push_back(ROMFS_PATH);
        std::vector<std::string> res;
        for (const auto& path : payloadPaths) {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension().string() == ".bin") {
                    if (entry.path().string() != FUSEE_SECONDARY && entry.path().string() != FUSEE_MTC)
                        res.push_back(entry.path().string().c_str());
                }
            }
        }
        return res;
    }

    void shutDown(bool reboot)
    {
        spsmInitialize();
        spsmShutdown(reboot);
    }

    void rebootToPayload(const std::string& path)
    {
        reboot_to_payload(path.c_str(), CurrentCfw::running_cfw != CFW::ams);
    }

    std::string getLatestTag(const std::string& url)
    {
        nlohmann::ordered_json tag;
        download::getRequest(url, tag, {"accept: application/vnd.github.v3+json"});
        if (tag.find("tag_name") != tag.end())
            return tag["tag_name"];
        else
            return "";
    }

    std::string downloadFileToString(const std::string& url)
    {
        std::vector<uint8_t> bytes;
        download::downloadFile(url, bytes);
        std::string str(bytes.begin(), bytes.end());
        return str;
    }

    std::string getCheatsVersion()
    {
        std::string res = util::downloadFileToString(CHEATS_URL_VERSION);
        /* if (res == "" && isArchive(CHEATS_FILENAME)) {
            res = "offline";
        } */
        return res;
    }

    void saveToFile(const std::string& text, const std::string& path)
    {
        std::ofstream file(path);
        file << text << std::endl;
    }

    std::string readFile(const std::string& path)
    {
        std::string text = "";
        std::ifstream file(path);
        if (file.good()) {
            file >> text;
        }
        return text;
    }

    std::string getAppPath()
    {
        if (envHasArgv()) {
            std::smatch match;
            std::string argv = (char*)envGetArgv();
            if (std::regex_match(argv, match, std::regex(NRO_PATH_REGEX))) {
                if (match.size() >= 2) {
                    return match[1].str();
                }
            }
        }
        return NRO_PATH;
    }

    void restartApp()
    {
        std::string path = "sdmc:" + getAppPath();
        std::string argv = "\"" + path + "\"";
        envSetNextLoad(path.c_str(), argv.c_str());
        romfsExit();
        brls::Application::quit();
    }

    bool isErista()
    {
        SetSysProductModel model;
        setsysGetProductModel(&model);
        return (model == SetSysProductModel_Nx || model == SetSysProductModel_Copper);
    }

    void removeSysmodulesFlags(const std::string& directory)
    {
        for (const auto& e : std::filesystem::recursive_directory_iterator(directory)) {
            if (e.path().string().find("boot2.flag") != std::string::npos) {
                std::filesystem::remove(e.path());
            }
        }
    }

    std::string lowerCase(const std::string& str)
    {
        std::string res = str;
        std::for_each(res.begin(), res.end(), [](char& c) {
            c = std::tolower(c);
        });
        return res;
    }

    std::string upperCase(const std::string& str)
    {
        std::string res = str;
        std::for_each(res.begin(), res.end(), [](char& c) {
            c = std::toupper(c);
        });
        return res;
    }

    std::string getErrorMessage(long status_code)
    {
        std::string res;
        switch (status_code) {
            case 500:
                res = fmt::format("{0:}: Internal Server Error", status_code);
                break;
            case 503:
                res = fmt::format("{0:}: Service Temporarily Unavailable", status_code);
                break;
            default:
                res = fmt::format("error: {0:}", status_code);
                break;
        }
        return res;
    }

    bool isApplet()
    {
        AppletType at = appletGetAppletType();
        return at != AppletType_Application && at != AppletType_SystemApplication;
    }

    std::set<std::string> getExistingCheatsTids()
    {
        std::string path = getContentsPath();
        std::set<std::string> res;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::string cheatsPath = entry.path().string() + "/cheats";
            if (std::filesystem::exists(cheatsPath)) {
                res.insert(util::upperCase(cheatsPath.substr(cheatsPath.length() - 7 - 16, 16)));
            }
        }
        return res;
    }

    std::string getContentsPath()
    {
        std::string path;
        switch (CurrentCfw::running_cfw) {
            case CFW::ams:
                path = std::string(AMS_PATH) + std::string(CONTENTS_PATH);
                break;
        }
        return path;
    }

    bool getBoolValue(const nlohmann::ordered_json& jsonFile, const std::string& key)
    {
        return (jsonFile.find(key) != jsonFile.end()) ? jsonFile.at(key).get<bool>() : false;
    }

    const nlohmann::ordered_json getValueFromKey(const nlohmann::ordered_json& jsonFile, const std::string& key)
    {
        return (jsonFile.find(key) != jsonFile.end()) ? jsonFile.at(key) : nlohmann::ordered_json::object();
    }

    int openWebBrowser(const std::string url)
    {
        Result rc = 0;
        int at = appletGetAppletType();
        if (at == AppletType_Application) {  // Running as a title
            WebCommonConfig conf;
            WebCommonReply out;
            rc = webPageCreate(&conf, url.c_str());
            if (R_FAILED(rc))
                return rc;
            webConfigSetJsExtension(&conf, true);
            webConfigSetPageCache(&conf, true);
            webConfigSetBootLoadingIcon(&conf, true);
            webConfigSetWhitelist(&conf, ".*");
            rc = webConfigShow(&conf, &out);
            if (R_FAILED(rc))
                return rc;
        }
        else {  // Running under applet
            showDialogBoxInfo("menus/utils/applet_webbrowser"_i18n);
        }
        return rc;
    }

}  // namespace util
