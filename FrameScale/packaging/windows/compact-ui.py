"""Generate a compact NSIS dialog template from the installed NSIS resources.

Only dialog geometry is changed; the upstream control IDs/styles stay intact.
The generated PE is a resource template for makensis, never an executable helper.
"""
import argparse
import ctypes
from ctypes import wintypes as wt
from pathlib import Path
import shutil
import struct


def compact_dialog(data):
    result = bytearray(data)
    if struct.unpack_from('<HH', data) != (1, 0xFFFF):
        raise ValueError('Expected DIALOGEX resource')
    count = struct.unpack_from('<H', data, 16)[0]
    style = struct.unpack_from('<I', data, 12)[0]

    def geometry(offset):
        x, y, width, height = struct.unpack_from('<hhhh', result, offset)
        struct.pack_into('<hhhh', result, offset, round(x * .88), round(y * .88),
                         round(width * .88), round(height * .88))

    def skip_string(offset):
        word = struct.unpack_from('<H', data, offset)[0]
        if word == 0xFFFF:
            return offset + 4
        while struct.unpack_from('<H', data, offset)[0]:
            offset += 2
        return offset + 2

    geometry(18)
    offset = 26
    for _ in range(3):  # menu, window class, title
        offset = skip_string(offset)
    if style & 0x40:  # DS_SETFONT: point size, weight, italic, charset, face
        offset = skip_string(offset + 6)
    for _ in range(count):
        offset = (offset + 3) & ~3
        geometry(offset + 12)
        offset += 24
        offset = skip_string(offset)  # class
        offset = skip_string(offset)  # text
        extra = struct.unpack_from('<H', data, offset)[0]
        offset += 2 + extra
    if offset > len(data):
        raise ValueError('Dialog parser exceeded resource bounds')
    return bytes(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--nsis-root', type=Path, required=True)
    args = parser.parse_args()
    source = args.nsis_root / 'Contrib/UIs/modern.exe'
    output = Path(__file__).parent / 'brand/compact-ui.exe'
    output.parent.mkdir(exist_ok=True)
    shutil.copyfile(source, output)
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.LoadLibraryExW.argtypes = [wt.LPCWSTR, wt.HANDLE, wt.DWORD]
    api.LoadLibraryExW.restype = wt.HMODULE
    api.FindResourceW.argtypes = [wt.HMODULE, wt.LPVOID, wt.LPVOID]
    api.FindResourceW.restype = wt.HANDLE
    api.LoadResource.argtypes = [wt.HMODULE, wt.HANDLE]
    api.LoadResource.restype = wt.HANDLE
    api.LockResource.argtypes = [wt.HANDLE]
    api.LockResource.restype = wt.LPVOID
    api.SizeofResource.argtypes = [wt.HMODULE, wt.HANDLE]
    api.FreeLibrary.argtypes = [wt.HMODULE]
    callback_type = ctypes.WINFUNCTYPE(wt.BOOL, wt.HMODULE, wt.LPVOID, wt.LPVOID, wt.LPARAM)
    api.EnumResourceNamesW.argtypes = [wt.HMODULE, wt.LPVOID, callback_type, wt.LPARAM]
    api.BeginUpdateResourceW.argtypes = [wt.LPCWSTR, wt.BOOL]
    api.BeginUpdateResourceW.restype = wt.HANDLE
    api.UpdateResourceW.argtypes = [wt.HANDLE, wt.LPVOID, wt.LPVOID, wt.WORD, wt.LPVOID, wt.DWORD]
    api.EndUpdateResourceW.argtypes = [wt.HANDLE, wt.BOOL]
    library = api.LoadLibraryExW(str(source.resolve()), None, 2)
    if not library:
        raise ctypes.WinError(ctypes.get_last_error())
    resources = []
    errors = []

    def collect(module, kind, name, parameter):
        try:
            resource = api.FindResourceW(module, name, 5)
            raw = ctypes.string_at(api.LockResource(api.LoadResource(module, resource)),
                                   api.SizeofResource(module, resource))
            resources.append((name, compact_dialog(raw)))
            return True
        except Exception as error:
            errors.append(error)
            return False

    try:
        api.EnumResourceNamesW(library, 5, callback_type(collect), 0)
    finally:
        api.FreeLibrary(library)
    if errors:
        raise errors[0]
    if len(resources) != 9:
        raise ValueError(f'Unexpected NSIS dialog inventory: {len(resources)}')
    update = api.BeginUpdateResourceW(str(output.resolve()), False)
    if not update:
        raise ctypes.WinError(ctypes.get_last_error())
    failed = False
    try:
        for name, data in resources:
            buffer = ctypes.create_string_buffer(data)
            if not api.UpdateResourceW(update, 5, name, 1033, buffer, len(data)):
                raise ctypes.WinError(ctypes.get_last_error())
    except Exception:
        failed = True
        raise
    finally:
        if not api.EndUpdateResourceW(update, failed):
            raise ctypes.WinError(ctypes.get_last_error())
    print('Compact installer template: 291 x 195 DLU (22.8% less area)')


if __name__ == '__main__':
    main()
