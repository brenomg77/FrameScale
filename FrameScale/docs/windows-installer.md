# Instalador Windows

O instalador inclui a aplicação Release, Qt, bibliotecas do compilador, motores de processamento, modelos e avisos de terceiros. O outro computador não precisa de ferramentas de desenvolvimento. Os motores de IA exigem GPU/driver com Vulkan.

Ficheiro: `dist/FrameScale-0.2.0-Windows-x64-Setup.exe`. Os ficheiros `.sha256` e `.build.json` ao lado identificam o pacote e a árvore utilizada na construção. Os testes finais ficam num registo `.validation.json` separado. O setup instala por utilizador em `%LOCALAPPDATA%/Programs/FrameScale`, cria atalhos no menu Iniciar e oferece um atalho opcional no ambiente de trabalho. Está disponível em português de Portugal, português do Brasil e inglês.

Feche a aplicação antes de instalar ou desinstalar. O setup rejeita um executável bloqueado antes de alterar os ficheiros. A desinstalação remove a lista de ficheiros fornecidos pelo pacote e preserva ficheiros alheios e preferências. O instalador não tem assinatura digital de editor. A distribuição pública requer resolver os registos de fontes/licenças indicados nos [avisos de terceiros](../THIRD_PARTY_NOTICES.md).

## Gerar novamente

Ambiente validado: Qt 6.11.2 MinGW x64, MinGW 13.1, CMake, Ninja e NSIS 3.11. Partindo da pasta `FrameScale`:

```powershell
$env:PATH = 'C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
.\scripts\build-installer.ps1 -BuildDir build/Release -KeepStaging
```

O script verifica os 65 artefactos de runtime/modelos e os 19 textos de licença, compila, faz deploy para uma pasta nova e verifica novamente os hashes. Confere que o executável empacotado corresponde ao build e só substitui o setup final quando a compilação NSIS termina. Um setup anterior encontrado é arquivado em `build/release-records/archive`. A pasta de staging é removida no fim, salvo com `-KeepStaging`. `-PackageDir` escolhe a pasta onde cada staging único será criado, não uma pasta a reutilizar como payload.

Para outro NSIS, passe `-MakeNsis C:/caminho/makensis.exe`. O compilador portátil usado encontra-se em `build/setup-tools/nsis-3.11`; não faz parte do setup. [NSIS 3.11 oficial](https://sourceforge.net/projects/nsis/files/NSIS%203/3.11/nsis-3.11.zip/download), SHA-256 do ZIP: `c7d27f780ddb6cffb4730138cd1591e841f4b7edb155856901cdf5f214394fa1`.

## Verificar instalação e remoção

```powershell
.\scripts\verify-installer.ps1 -Setup dist/FrameScale-0.2.0-Windows-x64-Setup.exe -WorkDir build/installer-verification -Payload build/installer/payload-ID-IMPRESSO-PELO-BUILD
```

Use o caminho real do payload que o build reteve. A verificação instala numa pasta nova dentro de `build`, compara todos os ficheiros com o payload, rejeita uma atualização com o executável bloqueado, reinstala e desinstala. Um ficheiro sentinela comprova a preservação de dados alheios. As entradas de registo e os atalhos FrameScale existentes são copiados antes do teste e restaurados no final.

A execução local de 05/10/2026 e o hash do setup estão documentados no [fecho das correções](correcoes-finais-2026-10-05.md). O teste local com PATH restrito ao Windows não substitui um ensaio numa máquina limpa nem no computador onde será feita a defesa da PAP.
