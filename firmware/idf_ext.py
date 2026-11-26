"""Project defaults for plain ``idf.py qemu monitor``.

Adapted from OPC-UA/autotests/esp32/idf_ext.py for the N16R8 board and HTTP demo.
ESP-IDF continues to generate the flash image from sdkconfig and flash_args.
"""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import shlex
import socket

from idf_py_actions.errors import FatalError


def http_port() -> int:
    value = os.environ.get("QEMU_HTTP_PORT") or "8081"
    if not value.isdecimal() or not 1 <= int(value) <= 65535:
        raise FatalError(f"QEMU_HTTP_PORT must be a TCP port between 1 and 65535: {value}")
    return int(value)


def check_ports(ports: list[tuple[str, int]]) -> None:
    # Retain bindings until all ports are checked, including duplicates.
    with ExitStack() as sockets:
        for name, port in ports:
            sock = sockets.enter_context(socket.socket())
            try:
                sock.bind(("127.0.0.1", port))
            except OSError as error:
                hint = (
                    "Close the existing QEMU or monitor using port 5555."
                    if name == "serial monitor" else
                    f"Choose another host port with {name}=PORT."
                )
                raise FatalError(
                    f"Cannot bind {name} on 127.0.0.1:{port}: {error}\n{hint}"
                ) from error


def action_extensions(base_actions, project_path):
    qemu_action = base_actions["actions"]["qemu"]
    run_qemu = qemu_action["callback"]
    background = False

    def global_callback(ctx, global_args, tasks):
        nonlocal background
        background = any(
            task.name in ("monitor", "gdb") or task.name.startswith("efuse-")
            for task in tasks
        )

    def qemu_with_defaults(action, ctx, args, **options):
        extra = shlex.split(options["qemu_extra_args"])
        memory_supplied = any(arg == "-m" or arg.startswith("-m=") for arg in extra)
        if not memory_supplied:
            memory = os.environ.get("QEMU_RAM_SIZE")
            if memory:
                if (not memory.endswith("M") or not memory[:-1].isdecimal()
                        or int(memory[:-1]) == 0):
                    raise FatalError("QEMU_RAM_SIZE must be a positive size in MiB, e.g. 8M")
            else:
                description = Path(args.build_dir) / "project_description.json"
                target = json.loads(description.read_text())["target"]
                if target == "esp32s3":
                    # Match N16R8 PSRAM and leave address space for flash mappings.
                    memory = "8M"
            if memory:
                extra = ["-m", memory] + extra

        network_supplied = any(arg == "-nic" or arg.startswith("-nic=") for arg in extra)
        port = None if network_supplied else http_port()
        ports = [] if port is None else [("QEMU_HTTP_PORT", port)]
        if background:
            ports.append(("serial monitor", 5555))
        check_ports(ports)

        if port is not None:
            extra += [
                "-nic",
                f"user,model=open_eth,hostfwd=tcp:127.0.0.1:{port}-:80",
            ]
            print(f"HTTP: http://127.0.0.1:{port}/")

        options["qemu_extra_args"] = shlex.join(extra)
        return run_qemu(action, ctx, args, **options)

    qemu_action["callback"] = qemu_with_defaults
    qemu_action["help"] += (
        " Default ESP32-S3 RAM: 8M; override with QEMU_RAM_SIZE."
        " Forward HTTP to 127.0.0.1:8081; override with QEMU_HTTP_PORT."
        " Explicit -m and -nic in --qemu-extra-args take precedence."
    )
    return {"version": "1", "global_action_callbacks": [global_callback]}
