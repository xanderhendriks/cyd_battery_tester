import argparse
import esptool
import os
import serial.tools.list_ports
import sys


def resource_path(relative_path):
    """
    Get absolute path to resource, works for dev and for PyInstaller.

    Args:
        relative_path (str): The relative path to the resource.

    Returns:
        str: The absolute path to the resource.
    """
    base_path = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base_path, relative_path)


def list_serial_ports():
    """
    Lists serial ports available on the system.

    Prints:
        str: Available serial ports, descriptions, and hardware IDs.
    """
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        print(f"{port}: {desc} [{hwid}]")


def main():
    parser = argparse.ArgumentParser(description="CYD battery tester firmware updater")
    parser.add_argument('--port', help="Specify the serial port for the battery tester")
    parser.add_argument('--list-ports', action='store_true', help="List all serial ports available on the system")
    args = parser.parse_args()

    command = []

    if args.list_ports:
        list_serial_ports()
        sys.exit(0)

    if args.port is not None:
        command.extend(['--port', args.port])

    command.extend([
        '--baud', '460800',
        'write_flash',
        '0x01000', resource_path('binaries/bootloader.bin'),
        '0x10000', resource_path('binaries/cyd_battery_tester.bin'),
        '0x8000', resource_path('binaries/partition-table.bin')
    ])
    esptool.main(command)


if __name__ == '__main__':
    main()
