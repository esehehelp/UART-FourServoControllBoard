"""PlatformIO extra script: `pio run -t upload` via uploader/ (#54).

Flow (see uploader/main.go): send DLM command 0xF0 to the board's USB-CDC
port -> wait for the WCH BootROM -> wchisp flash. The uploader binary is
built with `go build` on first use.

platformio.ini:
    upload_protocol = custom
    extra_scripts = pre:fscb_upload.py
Options: upload_port = <port> (default: auto-detect by USB VID/PID),
         upload_flags = -mode touch / -legacy (see `uploader -h`).
"""
import os
import subprocess

Import("env")  # type: ignore  # noqa: F821

_project = env.subst("$PROJECT_DIR")  # type: ignore  # noqa: F821
_uploader_dir = os.path.normpath(os.path.join(_project, "..", "uploader"))
_uploader = os.path.join(_uploader_dir, "bin", "uploader.exe" if os.name == "nt" else "uploader")


def _build_uploader(*_args, **_kwargs):
    if os.path.isfile(_uploader):
        return
    print("Building %s (go build)" % _uploader)
    subprocess.check_call(["go", "build", "-o", _uploader, "."], cwd=_uploader_dir)


def _wchisp_path():
    try:
        pkg = env.PioPlatform().get_package_dir("tool-wchisp")  # type: ignore  # noqa: F821
    except Exception:
        pkg = None
    if not pkg:
        return None  # uploader then searches PATH / ~/.platformio / ~/.cargo
    for name in ("wchisp.exe", "wchisp"):
        path = os.path.join(pkg, name)
        if os.path.isfile(path):
            return path
    return None


if env.GetProjectOption("upload_protocol", "") == "custom":  # type: ignore  # noqa: F821
    _cmd = '"%s"' % _uploader
    _wchisp = _wchisp_path()
    if _wchisp:
        _cmd += ' -wchisp "%s"' % _wchisp
    if env.GetProjectOption("upload_port", ""):  # type: ignore  # noqa: F821
        _cmd += ' -port "$UPLOAD_PORT"'
    _cmd += ' $UPLOAD_FLAGS "$SOURCE"'
    env.Replace(UPLOADCMD=_cmd)  # type: ignore  # noqa: F821
    env.AddPreAction("upload", _build_uploader)  # type: ignore  # noqa: F821
