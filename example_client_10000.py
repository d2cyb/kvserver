#!/usr/bin/env python3

import socket
import sys
import time
import random

PREDEFINED_KEYS = [
    "server_name",
    "color",
    "max_connections",
    "timeout",
    "debug_mode",
    "log_level",
    "cache_size",
    "port",
    "host",
    "database_url",
    "api_key",
    "secret_key",
    "version",
    "environment",
    "feature_flag",
    "retry_count",
]


def generate_random_value():
    """Генерирует случайное значение для команды set."""
    value_types = [
        lambda: str(random.randint(1, 1000)),
        lambda: random.choice(["true", "false"]),
        lambda: f"value_{random.randint(1, 100)}_{random.randint(1000, 9999)}",
        lambda: random.choice(["red", "blue", "green", "yellow", "black", "white"]),
        lambda: f"{random.uniform(1.0, 100.0):.2f}",
    ]
    return random.choice(value_types)()


def create_connection(host="localhost", port=8080, max_retries=10, retry_delay=5):
    """Создает соединение с сервером с поддержкой переподключения."""
    retry_count = 0

    while retry_count < max_retries:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)  # Таймаут на подключение

            print(f"Попытка подключения к {host}:{port}...")
            sock.connect((host, port))
            sock.settimeout(30)  # Увеличиваем таймаут для операций

            print("Соединение установлено успешно")
            return sock

        except (socket.error, ConnectionRefusedError, TimeoutError) as e:
            retry_count += 1
            if retry_count >= max_retries:
                print(f"Не удалось подключиться после {max_retries} попыток")
                raise

            print(
                f"Ошибка подключения: {e}. Повторная попытка {retry_count}/{max_retries} через {retry_delay} секунд..."
            )
            time.sleep(retry_delay)

    raise ConnectionError("Не удалось установить соединение с сервером")


def send_command_with_reconnect(sock, command, port=8080, host="localhost"):
    """Отправляет команду с поддержкой переподключения при разрыве."""
    max_retries = 3

    for attempt in range(max_retries):
        try:
            # Отправляем команду
            sock.sendall((command + "\n").encode())

            # Получаем ответ
            response = sock.recv(1024)

            return response.decode().strip()

        except (socket.error, BrokenPipeError, ConnectionResetError) as e:
            if attempt < max_retries - 1:
                print(f"Разрыв соединения: {e}. Переподключение...")
                try:
                    sock.close()
                except:
                    pass

                # Пытаемся переподключиться
                sock = create_connection(host, port)
                continue
            else:
                raise

    return "ERROR: Не удалось выполнить команду после нескольких попыток"


def main():
    port = 8080
    host = "localhost"
    if len(sys.argv) > 1:
        try:
            host = sys.argv[1] if len(sys.argv) > 2 else "localhost"
            port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
        except ValueError:
            print(f"Использование: python3 {sys.argv[0]} [хост] [порт]")
            print(f"Хост по умолчанию: localhost")
            print(f"Порт по умолчанию: 8080")
            sys.exit(1)

    print("=" * 60)
    print("Запуск клиента для тестирования kvserver")
    print(f"Порт сервера: {port}")
    print(f"Количество итераций: 10 000")
    print(f"Список ключей: {len(PREDEFINED_KEYS)} элементов")
    print("=" * 60)

    try:
        # Создаем соединение
        sock = create_connection(host, port)

        # Основной цикл выполнения команд
        total_iterations = 10000
        get_count = 0
        set_count = 0
        error_count = 0

        start_time = time.time()

        for i in range(1, total_iterations + 1):
            try:
                # Выбираем случайный ключ
                key = random.choice(PREDEFINED_KEYS)

                # Выбираем команду с вероятностью 99% get, 1% set
                if random.random() < 0.99:  # 99% вероятность
                    command = f"get {key}"
                    get_count += 1
                    operation = "GET"
                else:  # 1% вероятность
                    value = generate_random_value()
                    command = f"set {key}={value}"
                    set_count += 1
                    operation = "SET"

                # Отправляем команду
                response = send_command_with_reconnect(sock, command, port)

                # Выводим результат
                if i % 100 == 0 or i <= 10 or i >= total_iterations - 10:
                    print(
                        f"[{i:5d}/{total_iterations}] {operation} {key}: {response[:50]}{'...' if len(response) > 50 else ''}"
                    )

            except Exception as e:
                error_count += 1
                print(f"[{i:5d}/{total_iterations}] ОШИБКА: {e}")

                # Пытаемся восстановить соединение
                try:
                    sock.close()
                except:
                    pass

                try:
                    sock = create_connection(host, port)
                except Exception as reconnect_error:
                    print(f"Критическая ошибка переподключения: {reconnect_error}")
                    break

        # Закрываем соединение
        try:
            sock.close()
        except:
            pass

        end_time = time.time()
        total_time = end_time - start_time

        # Выводим статистику
        print("\n" + "=" * 60)
        print("СТАТИСТИКА ВЫПОЛНЕНИЯ:")
        print(f"Всего итераций: {total_iterations}")
        print(f"GET операций: {get_count} ({get_count/total_iterations*100:.1f}%)")
        print(f"SET операций: {set_count} ({set_count/total_iterations*100:.1f}%)")
        print(f"Ошибок: {error_count} ({error_count/total_iterations*100:.1f}%)")
        print(f"Общее время: {total_time:.2f} секунд")
        print(f"Среднее время на операцию: {total_time/total_iterations*1000:.2f} мс")
        print(f"Операций в секунду: {total_iterations/total_time:.1f}")
        print("=" * 60)

    except KeyboardInterrupt:
        print("\n\nРабота клиента прервана пользователем")
        sys.exit(0)
    except Exception as e:
        print(f"\nКритическая ошибка: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
