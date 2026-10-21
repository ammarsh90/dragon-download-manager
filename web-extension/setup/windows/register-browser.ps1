[CmdletBinding()]
param([switch]$Unregister)
$ErrorActionPreference = 'Stop'
$taskInstall = $PSScriptRoot
if (-not $Unregister -and -not (Test-Path -LiteralPath (Join-Path $taskInstall 'DragonBrowserHost.exe'))) {
    throw 'Run the copy of this script beside DragonBrowserHost.exe in the extracted Dragon application folder.'
}
$taskHosts = @{
    'Software\Google\Chrome\NativeMessagingHosts\net.mobile_dragon.download_manager' = 'dragon-host-chromium.json'
    'Software\Microsoft\Edge\NativeMessagingHosts\net.mobile_dragon.download_manager' = 'dragon-host-chromium.json'
    'Software\Mozilla\NativeMessagingHosts\net.mobile_dragon.download_manager' = 'dragon-host-firefox.json'
}
foreach ($taskEntry in $taskHosts.GetEnumerator()) {
    if ($Unregister) {
        $taskExisting = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($taskEntry.Key)
        if ($taskExisting) {
            $taskValue = $taskExisting.GetValue('')
            $taskExisting.Dispose()
            if ($taskValue -eq (Join-Path $taskInstall $taskEntry.Value)) {
                [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($taskEntry.Key, $false)
            }
        }
    } else {
        $taskManifest = Join-Path $taskInstall $taskEntry.Value
        if (-not (Test-Path -LiteralPath $taskManifest)) { throw "Missing Dragon host manifest: $taskManifest" }
        $taskKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($taskEntry.Key)
        $taskKey.SetValue('', $taskManifest, [Microsoft.Win32.RegistryValueKind]::String)
        $taskKey.Dispose()
    }
}
Write-Output 'Dragon browser connection updated for this Windows user.'
