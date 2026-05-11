# Notas ao revisor Adobe

ModaDXF e um plugin nativo C++ para Adobe Illustrator em Windows x64.

## Como validar

1. Instalar o pacote pelo Creative Cloud desktop.
2. Reiniciar o Adobe Illustrator.
3. Abrir um arquivo DXF de modelagem de moda com grupos baseados em `BLOCK`/`INSERT`.
4. Selecionar `ModaDXF Modaris DXF` no dialogo de formato.
5. Confirmar que a janela `ModaDXF processando Molde` aparece.
6. Confirmar que os grupos principais aparecem preservados no painel de camadas.
7. Repetir com outro DXF do mesmo fluxo.
8. Conferir logs em `%LOCALAPPDATA%\ModaDXF\logs`.

## Comportamento esperado

- O plugin aparece como filtro de importacao DXF no Illustrator.
- O plugin nao acessa rede.
- O plugin grava diagnosticos locais (`runtime.log` e JSON de importacao).
- O plugin nao deve criar `%LOCALAPPDATA%\AMC\ModaDXF\logs`.

## Instalacao sem administrador

O destino preferencial e per-user:

```text
%APPDATA%\Adobe\Adobe Illustrator 30 Settings\pt_BR\x64\Plug-ins\ModaDXF
```

Se o fluxo de revisao Adobe exigir destino global para plugin nativo, usar a instalacao gerenciada pelo Creative Cloud desktop e documentar essa exigencia na pagina da listagem.
