# newsdesk

**ФИО:** Абрютин Иван Дмитриевич  
**Номер решаемой задачи:** N6 "Издание" (системное программирование)  
**Принятые проектные решения:** [`docs/specification.md`](docs/specification.md)

Сервер и клиент для сбора донесений от источников с разным доверием. Сервер объединяет донесения в сюжеты и решает, когда сюжет пора печатать.

Сервер однопоточный: неблокирующие сокеты и один `poll()`, поэтому синхронизации нет вообще, а приём заметки - одна атомарная операция. Состояние держится в памяти и восстанавливается проигрыванием append-only журнала, так что перезапуск не теряет ни принятых заметок, ни недоставленных поздравлений.

## Структура проекта
- `src/core/` - платформонезависимая логика: сюжеты, доверие/затухание, реестр клиентов, статистика, движок приёма заметок, очереди исходящих.
- `src/protocol/` - JSON <-> структуры сообщений.
- `src/storage/` - формат журнала (WAL), кодирование/декодирование, проигрывание при старте.
- `src/platform/` - всё, что зависит от ОС: сокеты, `poll`, сигналы, файлы (сейчас только POSIX/Linux; Windows - задел на будущее).
- `src/server/`, `src/client/` - режимы `newsdesk server` / `newsdesk client`.
- `tests/` - unit-тесты (doctest), `scripts/e2e/` - сквозные сценарии (pytest).

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

## Сквозные сценарии (Python, pytest)

Запускают собранный `newsdesk` целиком как подпроцесс и общаются с ним по сокету.
Требуют сборки проекта (см. выше) и `pytest`.

```sh
python3 -m pytest scripts/e2e -v
```

## Конфиг сервера (`config/example.json`)

| Поле | Смысл |
| --- | --- |
| `threshold` | порог суммарного доверия для выпуска сюжета |
| `congratulated_k` | сколько первых подавших получают поздравление |
| `half_life_hours` | период полураспада доверия сюжета |
| `listen` | адрес сервера `host:port` |
| `journal_path` | путь к файлу журнала (WAL) |
| `outbox_max_size` | лимит очереди недоставленных сообщений на клиента |
| `clients` | список `{id, trust, role}`, `role` - `reporter` или `editor` |

## Запуск

```sh
./build/src/newsdesk server --config config/example.json
./build/src/newsdesk client --server 127.0.0.1 --port 7000 --client-id alice
```

Любое поле конфига сервера можно переопределить аргументом командной строки: `--listen`, `--threshold`, `--congratulated-k`, `--half-life-hours`, `--journal-path`, `--outbox-max-size`.

Клиент после подключения читает команды из stdin:
- `note <rubric>|<topic>` - отправить заметку с текущим временем события;
- `stats [client_id]` - своя статистика, либо чужая (только для роли `editor`);
- `bye` - корректно отключиться.