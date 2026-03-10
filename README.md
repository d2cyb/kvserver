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
 * [boost](https://github.com/google/googletest.git) - обработка json и работа с сетью и тесты


## Сборка проекта
Создать в корне проекта директорию build, перейти в неё и выполнить следующие команды.

```sh
cmake ..
cmake --build . --config Debug --target all
```

## Запуск программы
В корне проекта выполнить следующую команду:

```sh
./build/bin/kvserver ./config.txt
```

Либо, если в директории с исполняемым файлам уже есть config.txt, то можно запустить без параметров.

```sh
./build/bin/kvserver
```


## Запуск тестов

### Unit тесты
Для запуска тестов выполните следующую команду:

```sh
./build/bin/kvserver_unit
```

### Нагрузочные тесты
Для запуска тестов выполните следующую команду:

```sh
./build/bin/kvserver_bench
```

### Тесты памяти valgrind
Для запуска тестов выполните следующую команду:

```sh
(cd ./build/tests/unit && ctest -T memcheck)
```


## Пример работы программы
Запустить собранный сервер
```sh
./build/bin/kvserver 8080
```

Или собрать и запустить сервер в docker-контейере
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