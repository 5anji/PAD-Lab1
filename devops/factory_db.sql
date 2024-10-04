-- factory_db.sql
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    config_id INTEGER,
    user_id VARCHAR(50),
    status VARCHAR(50) DEFAULT 'Pending',
    estimated_delivery DATE
);
