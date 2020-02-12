
#include "config.hpp"
#include "db.hpp"
#include "options.hpp"
#include "util.hpp"

#include <osmium/util/verbose_output.hpp>

#include <iostream>

static Command command_testdb = {"testdb", "Test connection to the database."};

int main(int argc, char *argv[])
{
    try {
        Options options{command_testdb};
        options.parse_command_line(argc, argv);
        osmium::VerboseOutput vout{!options.quiet()};
        options.show_version(vout);

        vout << "Reading config from '" << options.config_file() << "'\n";
        Config config{options, vout};

        vout << "Connecting to database...\n";
        pqxx::connection db{config.db_connection()};

        pqxx::work txn{db};
        vout << "Database version: " << get_db_version(txn) << '\n';

        pqxx::result result =
            txn.exec("SELECT slot_name, database, confirmed_flush_lsn FROM "
                     "pg_replication_slots WHERE slot_type = 'logical' AND "
                     "plugin = 'osm-logical';");

        if (result.empty()) {
            vout << "Replication not enabled\n";
        } else {
            vout << "Active replication slots:\n";
            for (auto const &row : result) {
                vout << "  name=" << row[0] << " db=" << row[1]
                     << " lsn=" << row[2] << '\n';
            }
        }

        txn.commit();

        vout << "Done.\n";
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    return 0;
}
