#include <fmt/format.h>
#include <pqxx/pqxx>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <exception>

inline constexpr auto database = "yourdatabase";
inline constexpr auto user = "youruser";
inline constexpr auto password = "yourpassword";
inline constexpr auto addr = "127.0.0.1";
inline constexpr auto port = 5432;

int main() {
    try {
        // Initialize spdlog to log to the terminal with colored output
        auto logger = spdlog::stdout_color_mt("console");

        // Log start of the program
        spdlog::info("Starting PostgreSQL connection program");

        // Establish a connection to the database
        pqxx::connection conn(fmt::format("dbname={} "
                                          "user={} "
                                          "password={} "
                                          "hostaddr={} "
                                          "port={}",
                                          database,
                                          user,
                                          password,
                                          addr,
                                          port));

        if (conn.is_open()) {
            spdlog::info("Connected to database: {}", conn.dbname());
        } else {
            spdlog::error("Failed to connect to the database.");
            return 1;
        }

        // Create a transactional object
        pqxx::work txn{conn};

        // Execute a simple SELECT query
        pqxx::result res = txn.exec("SELECT * FROM your_table");

        // Log the number of rows returned
        spdlog::info("Query returned {} rows", res.size());

        // Process the result set
        for (auto const& row : res) {
            std::string row_data;
            for (auto const& field : row) {
                row_data += std::string(field.c_str()) + "\t";
            }
            spdlog::info("Row: {}", row_data);
        }

        // Commit the transaction
        txn.commit();

        spdlog::info("Disconnected from the database.");
    } catch (std::exception const& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }

    return 0;
}
