#!/usr/bin/env python3
"""
Пример клиента для тестирования config-server.
Используйте: python3 example_client.py <команда> [порт]
"""

import socket
import sys
import time


def send_command(command, host="localhost", port=8080):
    """Отправляет команду серверу и возвращает ответ."""
    try:
        # Создаем сокет
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)

        # Подключаемся к серверу
        sock.connect((host, port))

        # Отправляем команду
        sock.sendall((command + "\n").encode())

        # Получаем ответ
        response = sock.recv(1024)

        sock.close()
        return response.decode().strip()

    except Exception as e:
        return f"ERROR: {e}"


def main():
    if len(sys.argv) < 2:
        print("Использование: python3 example_client.py <команда> [хост] [порт]")
        print("Примеры:")
        print("  python3 example_client.py 'get tree'")
        print("  python3 example_client.py 'set color=red'")
        print("  python3 example_client.py 'get server_name' 9000")
        sys.exit(1)

    command = sys.argv[1]
    host = sys.argv[2] if len(sys.argv) > 3 else "localhost"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 8080

    print(f"Отправка команды: {command}")
    print(f"Хост: {host}")
    print(f"Порт: {port}")
    print("-" * 40)

    response = send_command(command, host, port)
    print("Ответ сервера:")
    print(response)


if __name__ == "__main__":
    main()
