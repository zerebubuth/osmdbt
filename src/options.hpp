#pragma once

#include <boost/program_options.hpp>

#include <osmium/util/verbose_output.hpp>

#include <string>

namespace po = boost::program_options;

class Options
{
public:
    explicit Options(char const *name, char const *description)
    : m_name(name), m_description(description)
    {}

    virtual ~Options() = default;

    void parse_command_line(int argc, char *argv[]);

    bool quiet() const noexcept { return m_quiet; };

    std::string const &config_file() const noexcept { return m_config_file; }

    void show_version(osmium::VerboseOutput &vout);

private:
    void check_common_options(boost::program_options::variables_map const &vm,
                              po::options_description const &desc);

    virtual void add_command_options(po::options_description & /*desc*/) {}

    virtual void
    check_command_options(boost::program_options::variables_map const & /*vm*/)
    {}

    char const *m_name;
    char const *m_description;
    bool m_quiet = false;
    std::string m_config_file{"osmdbt-config.yaml"};
}; // class Options
