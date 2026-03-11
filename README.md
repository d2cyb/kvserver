# kvserver

## Описание
Сервер ключ/значение.

### Файла настроек
Формат файла конфигурации - JSON.

Пример:
```json
{
    "key1": "value1",
    "key2": "1",
    "key3": "value 3"
}
```


## Зависимости проекта
 * [boost](https://github.com/google/googletest.git) - обработка json и работа с сетью
 * [catch2](https://github.com/catchorg/Catch2) - unit и benchmark тесты


## Сборка проекта
Для сборки release версии проекта выполнить следующие команды из корня проекта:

```sh
cmake -S . -B build/release --preset ci-release
cmake --build build/release --target all --
```

## Запуск программы
В корне проекта выполнить следующую команду:

```sh
./build/release/bin/kvserver 8080 ./config.txt
```
Остановить сервер командой Ctrl+C. После остановки таймера 1-5 секунд, сервер завершит работу.


## Запуск тестов

### Unit тесты
Для запуска модульных тестов, из корня проекта выполните следующую команду:

```sh
./build/release/bin/kvserver_unit
```

### Нагрузочные тесты
Для запуска benchmark тестов, из корня проекта выполните следующую команду:

```sh
./build/release/bin/kvserver_bench
```

### Тесты памяти valgrind
Для запуска тестов памяти, из корня проекта выполните следующую команду:

```sh
cmake --build build/release --target sanitize-memory

```


## Пример работы программы
Запустить собранный сервер
```sh
./build/bin/kvserver 8080
```

Или собрать и запустить сервер в docker-контейнере. Для этого выполнить следующие команды из корня проекта.
```sh
docker build -t kvserver-release --target release .
docker run --rm -p 8080:8080 -v ./config.txt:/app/config.txt kvserver-release:latest
```

Выполнить запрос с помощью тестового клиента example_client:
```sh
python ./example_client.py "get key1"
```

Проверить работу сервера можно запустив тестовый клиент. Клиент будет выполнять 10000 запросов, из них 90% get и 10% set.
```sh
python ./example_client_10000.py
```