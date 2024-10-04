# Compile options

```bash
g++ main.cpp -std=c++20 -lpqxx -lpq -lspdlog -lfmt -lhiredis -lredis++ -Wall -Wextra -O3
```

## Testing

POST:

```bash
curl -X POST http://localhost:8080/api/configure -H "Content-Type: application/json" -d '{
  "engine": "V6",
  "color": "Red",
  "interior": "Leather",
  "features": ["Sunroof", "GPS"]
}'
```

GET:

```bash
curl -X GET http://localhost:8080/api/configure/1
```
