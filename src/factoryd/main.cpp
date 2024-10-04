#include <nlohmann/json.hpp>
#include <pqxx/pqxx>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sw/redis++/redis++.h>

#include <crow.h>

#include <exception>
#include <thread>

using json = nlohmann::json;

inline constexpr auto database = "factory_db";
inline constexpr auto user = "factory_user";
inline constexpr auto password = "factory_pass";
inline constexpr auto addr = "127.0.0.1";
inline constexpr auto port = 5433;

inline constexpr auto redis_host = "127.0.0.1";
inline constexpr auto redis_port = 6379;

inline constexpr auto http_port = 8081;

int main() {
    try {
        // Logger setup
        auto logger = spdlog::stdout_color_mt("console");
        spdlog::info("Starting Factory Service");

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

        // Endpoint to place an order
        CROW_ROUTE(app, "/api/factory/order")
                .methods("POST"_method)([&](crow::request const& req) {
                    json j = json::parse(req.body);
                    std::string config_id = j["configId"];
                    std::string user_id = j["userId"];

                    // Insert into database
                    pqxx::work txn{conn};
                    auto result = txn.exec_params(
                            "INSERT INTO orders (config_id, user_id) VALUES ($1, "
                            "$2) RETURNING order_id",  // use fmt later
                            config_id,
                            user_id);
                    txn.commit();

                    auto order_id = result[0][0].as<int>();

                    // Generate a response
                    json response = {{"orderId", order_id},
                                     {"message", "Order placed successfully."}};
                    return crow::response{response.dump()};
                });

        // Endpoint to get order status by ID
        CROW_ROUTE(app, "/api/factory/order/<string>")
                .methods("GET"_method)([&]([[maybe_unused]] crow::request const& req,
                                           std::string orderId) {
                    // Check Redis cache
                    std::string cache_key = "order:" + orderId;
                    std::string cached_data = redis_client.get(cache_key).value_or("");

                    if (cached_data.empty()) {
                        spdlog::info(
                                "Order status not found in Redis, querying database.");
                        pqxx::work txn{conn};
                        pqxx::result res = txn.exec_params(
                                "SELECT status FROM orders WHERE order_id = $1",
                                orderId);

                        if (res.empty()) {
                            return crow::response{404, "Order not found"};
                        }

                        auto row = res[0];
                        nlohmann::json json;
                        json["status"] = row["status"].as<std::string>();

                        // Cache the result
                        cached_data = json.dump(4);
                        redis_client.set(cache_key, cached_data);
                        spdlog::info("Order status cached in Redis.");
                    } else {
                        spdlog::info("Order status retrieved from Redis: {}",
                                     cached_data);
                    }

                    return crow::response{cached_data};  // Return cached data
                });

        // Start the HTTP server
        app.port(http_port).multithreaded().run();

    } catch (std::exception const& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    return 0;
}
