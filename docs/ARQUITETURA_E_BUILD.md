# Arquitetura e Build

## Visao geral

ModaDXF e um plugin nativo C++ para Adobe Illustrator. A arquitetura separa a leitura semantica do DXF da criacao de arte no Illustrator.

Fluxo principal:

1. `DxfParser` le o arquivo DXF.
2. `RenderPlan` transforma o modelo DXF em um plano de importacao.
3. `ModaDxfImportFilter` registra o formato `ModaDXF Modaris DXF`.
4. `ModaDxfIllustratorPlugin` cria grupos, caminhos, textos e pontos via Illustrator SDK.
5. O diagnostico local e gravado em `%LOCALAPPDATA%\ModaDXF\logs`.

## Componentes

- `src/core`: parser DXF e geracao do plano deterministico de renderizacao.
- `src/diagnostic`: executavel CLI para validar arquivos DXF sem abrir Illustrator.
- `src/illustrator`: integracao com Adobe Illustrator SDK e filtro de importacao.
- `build`: projetos MSBuild/Visual Studio.
- `msi`: pacote WiX per-user para validacao local.
- `adobe-distribution`: material para Adobe Developer Distribution.

## Dependencia do Adobe Illustrator SDK

O SDK da Adobe nao e versionado aqui. Para compilar o `.aip`, baixe o SDK no portal Adobe e defina:

```powershell
$env:AI_SDK_ROOT = "C:\caminho\para\Adobe Illustrator SDK"
```

O projeto usa `AI_SDK_ROOT` para localizar headers, libs, exemplos comuns e a ferramenta de geracao PiPL.

## Empacotamento

O MSI local instala em escopo de usuario, evitando permissao administrativa quando o Illustrator aceita plugin no perfil:

```text
%APPDATA%\Adobe\Adobe Illustrator 30 Settings\pt_BR\x64\Plug-ins\ModaDXF
```

Para distribuicao publica, o fluxo previsto e ZXP/MXI pelo Adobe Developer Distribution.

## Logs

O plugin grava:

```text
%LOCALAPPDATA%\ModaDXF\logs
```

Os logs sao locais e nao ha envio de telemetria.
