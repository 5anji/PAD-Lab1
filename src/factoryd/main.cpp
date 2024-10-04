#include <pqxx/pqxx>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sw/redis++/redis++.h>

#include <exception>
#include <semaphore>
#include <thread>

inline constexpr auto database = "factory_db";
inline constexpr auto user = "factory_user";
inline constexpr auto password = "factory_pass";
inline constexpr auto addr = "127.0.0.1";
inline constexpr auto port = 5432;

inline constexpr auto redis_host = "127.0.0.1";
inline constexpr auto redis_port = 6379;

inline constexpr int max_concurrent_tasks = 5;
std::counting_semaphore<max_concurrent_tasks> task_semaphore(max_concurrent_tasks);

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

        // Cache order status example
        std::string order_key = "order:567";
        std::string order_status = redis_client.get(order_key).value_or("");

        if (order_status.empty()) {
            spdlog::info("Order status not found in Redis, querying database.");
            pqxx::work txn{conn};
            pqxx::result res = txn.exec("SELECT status FROM orders WHERE order_id = 567");
            if (!res.empty()) {
                order_status = res[0]["status"].c_str();
                redis_client.set(order_key, order_status);
                spdlog::info("Order status cached in Redis.");
            } else {
                spdlog::info("No order found in database.");
            }
        } else {
            spdlog::info("Order status retrieved from Redis: {}", order_status);
        }

        // Concurrency control
        task_semaphore.acquire();
        // Simulate long-running task
        std::thread([&conn] {
            try {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                pqxx::work txn{conn};
                pqxx::result res =
                        txn.exec("SELECT * FROM orders WHERE status = 'In Production'");
                spdlog::info("Order status fetched.");
            } catch (std::exception& e) {
                spdlog::error("Task Timeout or Error: {}", e.what());
            }
        }).detach();
        task_semaphore.release();

    } catch (std::exception const& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    return 0;
}
