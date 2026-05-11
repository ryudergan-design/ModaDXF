param(
  [Parameter(Mandatory = $true)]
  [string]$MsiPath
)

$ErrorActionPreference = "Stop"

$resolvedMsi = (Resolve-Path -LiteralPath $MsiPath).Path
$installer = New-Object -ComObject WindowsInstaller.Installer
$database = $installer.OpenDatabase($resolvedMsi, 1)

function Escape-MsiSql([string]$Value) {
  return $Value.Replace("'", "''")
}

function Set-ControlText([string]$Dialog, [string]$Control, [string]$Text) {
  $safeText = Escape-MsiSql $Text
  $safeDialog = Escape-MsiSql $Dialog
  $safeControl = Escape-MsiSql $Control
  $view = $database.OpenView("UPDATE ``Control`` SET ``Text``='$safeText' WHERE ``Dialog_``='$safeDialog' AND ``Control``='$safeControl'")
  $view.Execute()
  $view.Close()
}

function Set-PropertyValue([string]$Name, [string]$Value) {
  $safeName = Escape-MsiSql $Name
  $safeValue = Escape-MsiSql $Value
  $view = $database.OpenView("UPDATE ``Property`` SET ``Value``='$safeValue' WHERE ``Property``='$safeName'")
  $view.Execute()
  $view.Close()
}

Set-ControlText "WelcomeDlg" "Title" "{\WixUI_Font_Bigger}Bem-vindo ao instalador do [ProductName]"
Set-ControlText "WelcomeDlg" "Description" "Este assistente vai instalar o [ProductName] no Adobe Illustrator."
Set-ControlText "WelcomeDlg" "Next" "Avançar"
Set-ControlText "WelcomeDlg" "Cancel" "Cancelar"

Set-ControlText "LicenseAgreementDlg" "Title" "{\WixUI_Font_Title}Moda DXF - Documentação"
Set-ControlText "LicenseAgreementDlg" "Description" "Leia a documentação do Moda DXF antes de continuar."
Set-ControlText "LicenseAgreementDlg" "LicenseAcceptedCheckBox" "Li e entendi a documentação do Moda DXF"
Set-ControlText "LicenseAgreementDlg" "Print" "Imprimir"
Set-ControlText "LicenseAgreementDlg" "Back" "Voltar"
Set-ControlText "LicenseAgreementDlg" "Next" "Avançar"
Set-ControlText "LicenseAgreementDlg" "Cancel" "Cancelar"

Set-ControlText "InstallDirDlg" "Title" "{\WixUI_Font_Title}Pasta de instalação"
Set-ControlText "InstallDirDlg" "Description" "Clique em Avançar para usar a pasta padrão ou em Alterar para escolher outra."
Set-ControlText "InstallDirDlg" "FolderLabel" "Instalar [ProductName] em:"
Set-ControlText "InstallDirDlg" "ChangeFolder" "Alterar..."
Set-ControlText "InstallDirDlg" "Back" "Voltar"
Set-ControlText "InstallDirDlg" "Next" "Avançar"
Set-ControlText "InstallDirDlg" "Cancel" "Cancelar"

Set-ControlText "VerifyReadyDlg" "InstallTitle" "{\WixUI_Font_Title}Pronto para instalar o [ProductName]"
Set-ControlText "VerifyReadyDlg" "InstallText" "Clique em Instalar para adicionar o Moda DXF ao Adobe Illustrator."
Set-ControlText "VerifyReadyDlg" "ChangeTitle" "{\WixUI_Font_Title}Pronto para alterar o [ProductName]"
Set-ControlText "VerifyReadyDlg" "ChangeText" "Clique em Alterar para atualizar as opções da instalação."
Set-ControlText "VerifyReadyDlg" "RepairTitle" "{\WixUI_Font_Title}Pronto para reparar o [ProductName]"
Set-ControlText "VerifyReadyDlg" "RepairText" "Clique em Reparar para corrigir a instalação atual."
Set-ControlText "VerifyReadyDlg" "RemoveTitle" "{\WixUI_Font_Title}Pronto para remover o [ProductName]"
Set-ControlText "VerifyReadyDlg" "RemoveText" "Clique em Remover para desinstalar o Moda DXF."
Set-ControlText "VerifyReadyDlg" "UpdateTitle" "{\WixUI_Font_Title}Pronto para atualizar o [ProductName]"
Set-ControlText "VerifyReadyDlg" "UpdateText" "Clique em Atualizar para instalar a versão nova do Moda DXF."
Set-ControlText "VerifyReadyDlg" "Back" "Voltar"
Set-ControlText "VerifyReadyDlg" "Install" "Instalar"
Set-ControlText "VerifyReadyDlg" "InstallNoShield" "Instalar"
Set-ControlText "VerifyReadyDlg" "Change" "Alterar"
Set-ControlText "VerifyReadyDlg" "ChangeNoShield" "Alterar"
Set-ControlText "VerifyReadyDlg" "Repair" "Reparar"
Set-ControlText "VerifyReadyDlg" "Remove" "Remover"
Set-ControlText "VerifyReadyDlg" "RemoveNoShield" "Remover"
Set-ControlText "VerifyReadyDlg" "Update" "Atualizar"
Set-ControlText "VerifyReadyDlg" "UpdateNoShield" "Atualizar"
Set-ControlText "VerifyReadyDlg" "Cancel" "Cancelar"

Set-ControlText "ExitDialog" "Title" "{\WixUI_Font_Bigger}[ProductName] instalado"
Set-ControlText "ExitDialog" "Description" "Clique em Concluir para sair do instalador."
Set-ControlText "ExitDialog" "Back" "Voltar"
Set-ControlText "ExitDialog" "Finish" "Concluir"
Set-ControlText "ExitDialog" "Cancel" "Cancelar"

Set-PropertyValue "WIXUI_EXITDIALOGOPTIONALTEXT" "Instalação concluída. O Moda DXF foi adicionado ao Illustrator e está pronto para abrir Moldes em DXF. O resumo da instalação pode abrir agora."
Set-PropertyValue "WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT" "Abrir a documentação da instalação"

$database.Commit()
Write-Host "Interface do MSI ajustada: $resolvedMsi"
