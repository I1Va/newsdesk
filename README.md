# newsdesk

Сервер и клиент для сбора донесений от источников с разным доверием. Сервер объединяет донесения в сюжеты и решает, когда сюжет пора печатать. Подробности - в [`docs/specification.md`](docs/specification.md).

## Требования
- Linux, компилятор с поддержкой C++17 (gcc/clang)
- CMake >= 3.16
- Доступ в интернет при первой настройке (зависимости `nlohmann/json` и `doctest` подгружаются через `FetchContent`)

## Сборка

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## Тесты

```sh
cd build
ctest --output-on-failure
```

## Запуск

```sh
./build/src/newsdesk server --config config/example.json
./build/src/newsdesk client --config config/example.json
```