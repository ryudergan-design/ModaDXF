# ModaDXF

ModaDXF e um plugin gratuito para Adobe Illustrator no Windows, criado para melhorar a importacao de arquivos DXF usados em modelagem de moda.

O plugin le o DXF diretamente, reconstrui grupos principais baseados em `BLOCK` e `INSERT`, preserva camadas quando possivel e gera logs locais em `%LOCALAPPDATA%\ModaDXF\logs` para diagnostico.

## Uso

1. Instale o ModaDXF.
2. Feche e reabra o Adobe Illustrator.
3. Abra ou importe um arquivo `.DXF`.
4. Quando o Illustrator perguntar o formato, selecione `ModaDXF Modaris DXF`.
5. Aguarde a janela de progresso e revise os grupos criados.

## O que esta neste repositorio

- `src/`: parser DXF, plano de renderizacao e adaptador do Illustrator SDK.
- `build/`: projetos Visual Studio/MSBuild para gerar o diagnosticador e o plugin `.aip`.
- `installer/`, `msi/`, `tools/`: instalador per-user para validacao local.
- `scripts/`: automacao de build, MSI e pacote Adobe ZXP.
- `adobe-distribution/`: metadados para submissao gratuita no Adobe Developer Distribution.
- `docs/`: arquitetura e processo de build.

## O que nao esta neste repositorio

Por seguranca/licenca, este repositorio nao inclui:

- Adobe Illustrator SDK.
- certificados, senhas ou chaves privadas.
- binarios gerados (`.aip`, `.exe`, `.msi`, `.zxp`).
- logs de diagnostico.
- arquivos DXF de teste ou dados de usuario.

## Build

Requisitos:

- Windows x64.
- Visual Studio 2022 com C++.
- .NET SDK para WiX.
- WiX Toolset via `msi/ModaDXFNative.wixproj`.
- Adobe Illustrator SDK local.
- Variavel `AI_SDK_ROOT` apontando para a raiz do SDK.

Compilar o diagnosticador:

```powershell
.\scripts\build-diagnostic.ps1
```

Compilar o plugin nativo:

```powershell
.\scripts\build-plugin.ps1
```

Gerar MSI/EXE local:

```powershell
.\scripts\build-msi.ps1
```

Gerar staging Adobe ZXP:

```powershell
.\scripts\package-adobe-zxp.ps1 -SkipSigning
```

Assinar ZXP, quando houver certificado:

```powershell
.\scripts\package-adobe-zxp.ps1 `
  -ZxpSignCmd "C:\Tools\ZXPSignCmd.exe" `
  -CertificatePath "<caminho-do-certificado.p12>" `
  -CertificatePassword "<senha>"
```

## Status

Projeto em preparacao para distribuicao gratuita pelo Adobe Creative Cloud Marketplace / Adobe Exchange.
