#include "GameConfig.hpp"
#include <cstdlib>
#include <cstring>
#include <rw/defines.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
namespace pt = boost::property_tree;

const std::string kConfigDirectoryName("OpenRW");

GameConfig::GameConfig(const std::string& configName,
                       const std::string& configPath)
    : m_configName(configName)
    , m_configPath(configPath)
    , m_valid(false)
    , m_inputInvertY(false) {
    if (m_configPath.empty()) {
        m_configPath = getDefaultConfigPath();
    }

    // Look up the path to use
    auto configFile = getConfigFile();

    m_valid = readConfig(configFile);
}

std::string GameConfig::getConfigFile() {
    return m_configPath + "/" + m_configName;
}

bool GameConfig::isValid() {
    return m_valid;
}

std::string GameConfig::getDefaultConfigPath() {
#if defined(RW_LINUX) || defined(RW_FREEBSD) || defined(RW_NETBSD) || \
    defined(RW_OPENBSD)
    char* config_home = getenv("XDG_CONFIG_HOME");
    if (config_home != nullptr) {
        return std::string(config_home) + "/" + kConfigDirectoryName;
    }
    char* home = getenv("HOME");
    if (home != nullptr) {
        return std::string(home) + "/.config/" + kConfigDirectoryName;
    }

#elif defined(RW_OSX)
    char* home = getenv("HOME");
    if (home)
        return std::string(home) + "/Library/Preferences/" +
               kConfigDirectoryName;

#else
    return ".";
#endif

    // Well now we're stuck.
    RW_ERROR("No default config path found.");
    return ".";
}

std::string stripComments(const std::string &str)
{
    auto s = std::string(str, 0, str.find_first_of(";#"));
    return s.erase(s.find_last_not_of(" \n\r\t")+1);
}

struct StringTranslator {
    typedef std::string internal_type;
    typedef std::string external_type;
    boost::optional<external_type> get_value(const internal_type &str) {
        return boost::optional<external_type>(stripComments(str));
    }
    boost::optional<internal_type> put_value(const external_type &str) {
        return boost::optional<internal_type>(str);
    }
};

struct BoolTranslator {
    typedef std::string internal_type;
    typedef bool        external_type;
    boost::optional<external_type> get_value(const internal_type &str) {
        return boost::optional<external_type>(std::stoi(stripComments(str)) != 0);
    }
    boost::optional<internal_type> put_value(const external_type &b) {
        return boost::optional<internal_type>(b ? "1" : "0");
    }
};

bool GameConfig::readConfig(const std::string &path) {
    pt::ptree config, defaultConfig;
    bool success = true;

    auto read_config = [&](const std::string &key, auto &target,
                          const auto &defaultValue, auto &translator,
                          bool optional=true) {
        typedef typename std::remove_reference<decltype(target)>::type targetType;
        defaultConfig.put(key, defaultValue, translator);
        try {
            target = config.get<targetType>(key, translator);
        } catch (pt::ptree_bad_path &e) {
            if (optional) {
              target = defaultValue;
            } else {
              success = false;
              RW_MESSAGE(e.what());
            }
        }
    };

    try {
        pt::read_ini(path, config);
    } catch (pt::ini_parser_error &e) {
        success = false;
        RW_MESSAGE(e.what());
    }

    auto deft = StringTranslator();
    auto boolt = BoolTranslator();

    // @todo Don't allow path seperators and relative directories
    read_config("game.path", this->m_gamePath, "/opt/games/Grand Theft Auto 3", deft, false);
    read_config("game.language", this->m_gameLanguage, "american", deft);

    read_config("input.invert_y", this->m_inputInvertY, false, boolt);

    if (!success) {
        std::stringstream strstream;
        pt::write_ini(strstream, defaultConfig);

        RW_MESSAGE("Failed to parse configuration file (" + path + ").");
        RW_MESSAGE("Create one looking like this at \"" + path + "\":");
        RW_MESSAGE(strstream.str());
    }

    return success;
}

