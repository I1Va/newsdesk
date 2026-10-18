# newsdesk

# ФИО: Абрютин Иван Дмитриевич
# Номер решаемой задачи: задание N6 "Издание" (системное программирование).
# Описание принятых проектных решений лежит в спецификации проекта `docs/specification.md` 

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

## clangd 
Сгенерировать `compile_commands.json`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Codestyle (clang-format)

Проверить, что форматирование не разошлось со стилем (ничего не меняет):

```sh
find src tests -name '*.h' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

Исправить форматирование на месте:

```sh
find src tests -name '*.h' -o -name '*.cpp' | xargs clang-format -i
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