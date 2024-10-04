FROM gcc:14
COPY . /usr/src/myapp
WORKDIR /usr/src/myapp
RUN g++ -o myapp -std=c++20 -lpqxx -lpq -lspdlog -lfmt main.cpp
CMD ["./myapp"]

# TODO add install of required libraries: redis-plus-plus, spdlog, fmt, psqlxx...

# TODO Integrate cmake
