-- config_db.sql
CREATE TABLE configurations (
    config_id SERIAL PRIMARY KEY,
    engine VARCHAR(50),
    color VARCHAR(50),
    interior VARCHAR(50),
    features TEXT[]  -- Array to store features like 'Sunroof', 'GPS', etc.
);
