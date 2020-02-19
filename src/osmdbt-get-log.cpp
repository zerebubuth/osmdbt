
#include "config.hpp"
#include "db.hpp"
#include "exception.hpp"
#include "io.hpp"
#include "util.hpp"

#include <osmium/io/detail/read_write.hpp>
#include <osmium/util/verbose_output.hpp>

#include <boost/optional.hpp>

#include <algorithm>
#include <ctime>
#include <iostream>
#include <iterator>
#include <string>
#include <cstdlib>

namespace {

/* Parse a PostgreSQL LSN as a 64-bit integer. The PG docs state that LSNs are
 * written as two 32-bit hex numbers separated by a slash.
 */
uint64_t parse_lsn(const char *s) {
    char *end = const_cast<char *>(s);
    const uint32_t a = std::strtoul(s, &end, 16);
    if (*end != '/') { throw std::invalid_argument("invalid LSN"); }
    ++end;
    const uint32_t b = std::strtoul(end, nullptr, 16);

    return (uint64_t(a) << 32) | uint64_t(b);
}

} // namespace


class GetLogOptions : public Options
{
public:
    GetLogOptions()
    : Options("get-log", "Write changes from replication slot to log file.")
    {}

    bool catchup() const noexcept { return m_catchup; }
    boost::optional<uint64_t> ignore_earlier_than() const noexcept {
        return m_ignore_earlier_than;
    }
    bool empty_is_ok() const noexcept { return m_empty_is_ok; }

private:
    void add_command_options(po::options_description &desc) override
    {
        po::options_description opts_cmd{"COMMAND OPTIONS"};

        // clang-format off
        opts_cmd.add_options()
            ("catchup", "Commit changes when they have been logged successfully");
        opts_cmd.add_options()
            ("ignore-earlier-than", po::value<std::string>(),
             "Ignore any commits with LSNs equal to or earlier than this.");
        opts_cmd.add_options()
            ("empty-is-ok", "An empty set of changes is OK, and the exit code "
             "can be zero in this case.");
        // clang-format on

        desc.add(opts_cmd);
    }

    void check_command_options(
        boost::program_options::variables_map const &vm) override
    {
        if (vm.count("catchup")) {
            m_catchup = true;
        }
        if (vm.count("ignore-earlier-than")) {
          std::string lsn_str = vm["ignore-earlier-than"].as<std::string>();
          m_ignore_earlier_than = parse_lsn(lsn_str.c_str());
        }
        if (vm.count("empty-is-ok")) {
            m_empty_is_ok = true;
        }
    }

    bool m_catchup = false;
    boost::optional<uint64_t> m_ignore_earlier_than;
    bool m_empty_is_ok = false;
}; // class GetLogOptions

static void write_data_to_file(std::string const &data,
                               std::string const &dir_name,
                               std::string const &file_name)
{
    std::string const file_name_final{dir_name + file_name};
    std::string const file_name_new{file_name_final + ".new"};

    int const fd = osmium::io::detail::open_for_writing(
        file_name_new, osmium::io::overwrite::no);

    osmium::io::detail::reliable_write(fd, data.data(), data.size());
    osmium::io::detail::reliable_fsync(fd);
    osmium::io::detail::reliable_close(fd);

    rename_file(file_name_new, file_name_final);
    sync_dir(dir_name);
}

static std::string get_time()
{
    std::string buffer(20, '\0');

    auto const t = std::time(nullptr);
    auto const num = std::strftime(&buffer[0], buffer.size(), "%Y%m%dT%H%M%S",
                                   std::localtime(&t));

    buffer.resize(num);
    assert(num == 15);

    return buffer;
}

bool app(osmium::VerboseOutput &vout, Config const &config,
         GetLogOptions const &options)
{
    PIDFile pid_file{config.run_dir(), "osmdbt-get-log"};

    vout << "Connecting to database...\n";
    pqxx::connection db{config.db_connection()};
    db.prepare("peek",
               "SELECT * FROM pg_logical_slot_peek_changes($1, NULL, NULL);");

    pqxx::work txn{db};
    vout << "Database version: " << get_db_version(txn) << '\n';

    vout << "Reading changes...\n";
    pqxx::result const result =
        txn.prepared("peek")(config.replication_slot()).exec();

    if (result.empty()) {
        vout << "No changes found.\n";
        return options.empty_is_ok();
    }

    vout << "There are " << result.size() << " changes.\n";

    std::string data;
    data.reserve(result.size() * 50); // log lines should fit in 50 bytes

    std::string lsn;

    for (auto const &row : result) {
        char const *const message = row[2].c_str();

        uint64_t lsn_val = parse_lsn(row[0].c_str());
        if (options.ignore_earlier_than() &&
          (lsn_val <= options.ignore_earlier_than())) {
            continue;
        }

        data.append(row[0].c_str());
        data += ' ';
        data.append(row[1].c_str());
        data += ' ';
        data.append(message);
        data += '\n';

        if (message[0] == 'C') {
            lsn = row[0].c_str();
        }
    }

    vout << "LSN is " << lsn << '\n';

    std::string lsn_dash;
    std::transform(lsn.cbegin(), lsn.cend(), std::back_inserter(lsn_dash),
                   [](char c) { return c == '/' ? '-' : c; });

    std::string file_name = "/osm-repl-";
    file_name += get_time();
    file_name += '-';
    file_name += lsn_dash;
    file_name += ".log";
    vout << "Writing log to '" << config.log_dir() << file_name << "'...\n";

    write_data_to_file(data, config.log_dir(), file_name);
    vout << "Wrote and synced log.\n";

    if (options.catchup()) {
        vout << "Catching up to " << lsn << "...\n";
        catchup_to_lsn(txn, config.replication_slot(), lsn);
    } else {
        vout << "Not catching up (use --catchup if you want this).\n";
    }

    txn.commit();

    vout << "Done.\n";

    return true;
}

int main(int argc, char *argv[])
{
    GetLogOptions options;
    return app_wrapper(options, argc, argv);
}
