# Compile options

```bash
g++ main.cpp -std=c++20 -lpqxx -lpq -lspdlog -lfmt -lhiredis -lredis++ -Wall -Wextra -O3
```

## Testing

POST:

```bash
curl -X POST http://localhost:8081/api/factory/order -H "Content-Type: application/json" -d '{
  "configId": "2",
  "userId": "u789"
}''
```

GET:

```bash
curl -X GET http://localhost:8081/api/factory/order/2
```
