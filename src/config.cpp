
#include "config.hpp"
#include "exception.hpp"

namespace {
void set_config(const YAML::Node &n, std::string &config) {
    if (n) {
        if (n.IsNull()) {
            config = "";
        } else {
            config = n.as<std::string>();
        }
    }
}

void build_str(std::string &str, const std::string &key, const std::string &val) {
    if (!val.empty()) {
        str += " ";
        str += key;
        str += "=";
        str += val;
    }
}
} // namespace

Config::Config(Options const &options, osmium::VerboseOutput &vout)
: m_config{YAML::LoadFile(options.config_file())}
{

    if (!m_config["database"]) {
        throw config_error{"Missing 'database' section."};
    }
    if (!m_config["database"].IsMap()) {
        throw config_error{"'database' entry must be a Map."};
    }

    set_config(m_config["database"]["host"], m_db_host);
    set_config(m_config["database"]["dbname"], m_db_dbname);
    set_config(m_config["database"]["user"], m_db_user);
    set_config(m_config["database"]["password"], m_db_password);

    build_str(m_db_connection, "host", m_db_host);
    build_str(m_db_connection, "dbname", m_db_dbname);
    build_str(m_db_connection, "user", m_db_user);
    build_str(m_db_connection, "password", m_db_password);

    set_config(m_config["database"]["replication_slot"], m_replication_slot);

    if (m_config["log_dir"]) {
        m_log_dir = m_config["log_dir"].as<std::string>();
    }

    if (m_config["changes_dir"]) {
        m_changes_dir = m_config["changes_dir"].as<std::string>();
    }

    if (m_config["run_dir"]) {
        m_changes_dir = m_config["run_dir"].as<std::string>();
    }

    vout << "Config:\n";
    vout << "  Database:\n";
    vout << "    Host: " << m_db_host << '\n';
    vout << "    Name: " << m_db_dbname << '\n';
    vout << "    User: " << m_db_user << '\n';
    vout << "    Password: (not shown)\n";
    vout << "    Replication Slot: " << m_replication_slot << '\n';
    vout << "  Directory for log files: " << m_log_dir << '\n';
    vout << "  Directory for change files: " << m_changes_dir << '\n';
    vout << "  Directory for run files: " << m_run_dir << '\n';
}

std::string const &Config::db_connection() const { return m_db_connection; }

std::string const &Config::replication_slot() const
{
    return m_replication_slot;
}

std::string const &Config::log_dir() const { return m_log_dir; }

std::string const &Config::changes_dir() const { return m_changes_dir; }

std::string const &Config::run_dir() const { return m_run_dir; }
