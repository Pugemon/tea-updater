#include <filesystem>
#include <switch.h>
#include <borealis.hpp>
#include <json.hpp>
#include "main_frame.hpp"
#include "constants.hpp"
#include "fs.hpp"
#include "current_cfw.hpp"
#include "warning_page.hpp"


namespace i18n = brls::i18n;
using namespace i18n::literals;

//TimeServiceType __nx_time_service_type = TimeServiceType_System;

//const CFW CurrentCfw::running_cfw = CurrentCfw::getCFW();
const CFW CurrentCfw::running_cfw = CFW::ams;

int main(int argc, char* argv[])
{
    // Init the app
    if (!brls::Application::init(APP_TITLE))
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }
    
    nlohmann::json languageFile = fs::parseJsonFile(LANGUAGE_JSON);
    if(languageFile.find("language") != languageFile.end())
        i18n::loadTranslations(languageFile["language"]);
    else
        i18n::loadTranslations();

    
    // Setup verbose logging on PC
#ifndef __SWITCH__
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);
#endif

    setsysInitialize();
    plInitialize(PlServiceType_User);
    nsInitialize();
    socketInitializeDefault();
    nxlinkStdio();
    splInitialize();
    romfsInit();
    
    fs::createTree(CONFIG_PATH);

    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);
    brls::Logger::debug("Start");

    if(std::filesystem::exists(HIDDEN_AIO_FILE)) {
        brls::Application::pushView(new MainFrame());
    }
    else {
        brls::Application::pushView(new WarningPage("menus/main/launch_warning"_i18n));
    }

    while (brls::Application::mainLoop());

    romfsExit();
    splExit();
    socketExit();
    nsExit();
    setsysExit();
    plExit();
    return EXIT_SUCCESS;
}
