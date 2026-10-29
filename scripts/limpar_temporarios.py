"""Limpeza conservadora do FrameScale. Sem argumentos, apenas simula.

Execute com --excluir para revisar a lista e confirmar a exclusao permanente.
Nao execute durante compilacao, empacotamento ou testes do instalador.
Preserva builds ativos, dist, modelos/runtime originais, ferramentas, Git,
docs, estudos, imagens, PSDs, output, tmp e registros de releases.
"""

import argparse
import os
from pathlib import Path
import re
import stat
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
STAGING = Path("FrameScale/build/installer")
DIRECTORY = re.compile(r"(?:payload|frontend)-[0-9a-f]{32}")
BACKEND = re.compile(r"backend-[0-9a-f]{32}\.exe")
# Apenas binarios copiados/gerados pelo build-installer.ps1. Nao apagamos
# documentos, imagens ou fontes de shaders, mesmo dentro do staging.
EXTENSIONS = {".exe", ".dll", ".bin", ".qm", ".param"}


def safe(path):
    """Recusa links, junctions e qualquer caminho fora da pasta autorizada."""
    try:
        relative = path.relative_to(ROOT)
        if relative.parts[:3] != STAGING.parts:
            return False
        current = ROOT
        for part in relative.parts:
            current /= part
            info = current.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
                return False
        path.resolve().relative_to((ROOT / STAGING).resolve())
        return True
    except (ValueError, OSError):
        return False


def tracked_files():
    git = ROOT / ".tools/MinGit/cmd/git.exe"
    result = subprocess.run(
        [str(git) if git.exists() else "git", "-C", str(ROOT), "ls-files", "-z"],
        check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return {os.fsdecode(name).replace("\\", "/").casefold()
            for name in result.stdout.split(b"\0") if name}


def candidates(tracked):
    base = ROOT / STAGING
    if not base.exists() or not safe(base):
        return []
    paths = []
    for entry in sorted(base.iterdir()):
        if not safe(entry):
            continue
        if entry.is_file() and BACKEND.fullmatch(entry.name):
            paths.append(entry)
        elif entry.is_dir() and DIRECTORY.fullmatch(entry.name):
            for directory, dirs, files in os.walk(entry, followlinks=False):
                dirs[:] = [name for name in dirs if safe(Path(directory) / name)]
                paths.extend(Path(directory) / name for name in files
                             if Path(name).suffix.lower() in EXTENSIONS)
    selected = []
    for path in paths:
        if not safe(path) or path.relative_to(ROOT).as_posix().casefold() in tracked:
            continue
        info = path.stat()
        # Arquivos recentes podem pertencer a um empacotamento em andamento.
        if time.time() - info.st_mtime < 3600:
            continue
        selected.append((path, info.st_size, info.st_mtime_ns))
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--excluir", action="store_true", help="pede confirmacao e exclui")
    parser.add_argument("--listar", action="store_true", help="mostra cada arquivo")
    args = parser.parse_args()
    if not (ROOT / "FrameScale/CMakeLists.txt").is_file():
        raise RuntimeError("Pasta do projeto nao reconhecida. Nada foi removido.")
    items = candidates(tracked_files())  # Falha fechada se Git nao estiver disponivel.
    groups = {}
    for path, size, _ in items:
        group = path.relative_to(ROOT / STAGING).parts[0]
        groups[group] = groups.get(group, 0) + size
        if args.listar:
            print(f"{size:12d}  {path.relative_to(ROOT)}")
    print(f"Projeto: {ROOT}\n\nSomente temporarios de montagem de instaladores:")
    for name, size in sorted(groups.items()):
        print(f"  {size / 1024**3:6.3f} GiB  {name}")
    total = sum(size for _, size, _ in items)
    print(f"\nTotal: {len(items)} arquivos; {total / 1024**3:.2f} GiB recuperaveis.")
    print("Preservados: docs, estudos, imagens/PSDs, codigo, Qt Creator, dist,\n"
          "modelos/runtime originais, ferramentas, tmp, output e registros de releases.")
    if not args.excluir or not items:
        print("\nSimulacao: nada removido. Use --excluir para confirmar a limpeza.")
        return
    print("\nFeche compilacoes, empacotamentos e testes de instalacao antes de continuar.")
    if input("Digite EXCLUIR para apagar permanentemente esses temporarios: ").strip() != "EXCLUIR":
        print("Cancelado. Nada removido.")
        return
    removed = freed = errors = 0
    for path, size, modified in items:
        try:
            if not safe(path):
                raise RuntimeError("caminho alterado ou link detectado")
            info = path.stat()
            if (info.st_size, info.st_mtime_ns) != (size, modified):
                raise RuntimeError("arquivo alterado desde a analise")
            path.unlink()  # Nunca remove diretorios recursivamente.
            removed += 1
            freed += size
        except (OSError, RuntimeError) as exc:
            errors += 1
            print(f"Preservado {path.relative_to(ROOT)}: {exc}")
    print(f"\nRemovidos: {removed}; liberados: {freed / 1024**3:.2f} GiB; ignorados: {errors}.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.SubprocessError, RuntimeError, EOFError, KeyboardInterrupt) as exc:
        print(f"Limpeza interrompida: {exc}")
        raise SystemExit(1)
