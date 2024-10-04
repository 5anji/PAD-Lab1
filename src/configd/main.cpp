#include <nlohmann/json.hpp>
#include <pqxx/pqxx>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sw/redis++/redis++.h>

#include <crow.h>

#include <exception>
#include <semaphore>
#include <thread>

using json = nlohmann::json;

inline constexpr auto database = "config_db";
inline constexpr auto user = "config_user";
inline constexpr auto password = "config_pass";
inline constexpr auto addr = "127.0.0.1";
inline constexpr auto port = 5432;

inline constexpr auto redis_host = "127.0.0.1";
inline constexpr auto redis_port = 6379;

inline constexpr int max_concurrent_tasks = 10;
std::counting_semaphore<max_concurrent_tasks> task_semaphore(max_concurrent_tasks);

// Function to convert PostgreSQL array to std::vector<std::string>
std::vector<std::string> parse_array(pqxx::field const& field) {
    std::vector<std::string> result;
    std::string array_string = field.as<std::string>();

    // Remove the surrounding curly braces
    if (array_string.front() == '{' && array_string.back() == '}') {
        array_string = array_string.substr(1, array_string.size() - 2);
    }

    // Split the string into items based on the comma
    std::string item;
    bool in_quotes = false;
    for (char c : array_string) {
        if (c == '"') {
            in_quotes = !in_quotes;  // Toggle the in_quotes flag
        }
        if (c == ',' && !in_quotes) {
            // End of an item, push it back if not empty
            if (!item.empty()) {
                // Remove surrounding whitespace and quotes
                item.erase(0, item.find_first_not_of(" \t\n\""));
                item.erase(item.find_last_not_of(" \t\n\"") + 1);
                result.push_back(item);
                item.clear();
            }
        } else {
            item += c;  // Append character to current item
        }
    }

    // Add the last item if it exists
    if (!item.empty()) {
        item.erase(0, item.find_first_not_of(" \t\n\""));  // Trim whitespace
        item.erase(item.find_last_not_of(" \t\n\"") + 1);  // Trim quotes
        result.push_back(item);
    }

    return result;
}

int main() {
    try {
        // Logger setup
        auto logger = spdlog::stdout_color_mt("console");
        spdlog::info("Starting Configuration Service");

        // PostgreSQL connection
        pqxx::connection conn(
                fmt::format("dbname={} user={} password={} hostaddr={} port={}",
                            database,
                            user,
                            password,
                            addr,
                            port));

        if (!conn.is_open()) {
            spdlog::error("Failed to connect to the database.");
            return 1;
        }

        // Redis connection
        sw::redis::ConnectionOptions redis_opts;
        redis_opts.host = redis_host;
        redis_opts.port = redis_port;
        auto redis_client = sw::redis::Redis(redis_opts);

        // Setup HTTP server
        crow::SimpleApp app;

        // Endpoint to configure
        CROW_ROUTE(app, "/api/configure")
                .methods("POST"_method)([&](crow::request const& req) {
                    json j = json::parse(req.body);
                    std::string engine = j["engine"];
                    std::string color = j["color"];
                    std::string interior = j["interior"];
                    std::vector<std::string> features = j["features"];

                    // Insert into database
                    pqxx::work txn{conn};
                    auto result = txn.exec_params(
                            "INSERT INTO configurations (engine, color, interior, "
                            "features) VALUES ($1, $2, $3, $4) RETURNING config_id",
                            engine,
                            color,
                            interior,
                            pqxx::to_string(features));
                    txn.commit();

                    auto config_id = result[0][0].as<int>();

                    // Generate a response
                    json response = {{"configId", std::to_string(config_id)},
                                     {"message", "Configuration saved successfully."}};
                    return crow::response{response.dump()};
                });

        // Endpoint to get configuration by ID
        CROW_ROUTE(app, "/api/configure/<string>")
                .methods("GET"_method)([&]([[maybe_unused]] crow::request const& req,
                                           std::string configId) {
                    // Check Redis cache
                    std::string cache_key = "config:" + configId;
                    std::string cached_data = redis_client.get(cache_key).value_or("");

                    if (cached_data.empty()) {
                        spdlog::info("Config not found in Redis, querying database.");
                        pqxx::work txn{conn};
                        pqxx::result res = txn.exec_params(
                                "SELECT * FROM configurations WHERE config_id = $1",
                                configId);

                        if (res.empty()) {
                            return crow::response{404, "Configuration not found"};
                        }

                        auto row = res[0];

                        nlohmann::json json;
                        // Retrieve data from the row and populate the JSON object
                        json["config_id"] = row["config_id"].as<int>();
                        json["engine"] = row["engine"].as<std::string>();
                        json["color"] = row["color"].as<std::string>();
                        json["interior"] = row["interior"].as<std::string>();
                        json["features"] = parse_array(row["features"]);

                        // Cache the result
                        cached_data = json.dump(4);
                        redis_client.set(cache_key, cached_data);
                        spdlog::info("Configuration cached in Redis.");
                    } else {
                        spdlog::info("Config retrieved from Redis: {}", cached_data);
                    }

                    return crow::response{cached_data};  // Return cached data
                });

        // Start the HTTP server
        app.port(8080).multithreaded().run();

    } catch (std::exception const& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    return 0;
}
