param(
    [string]$EngineRoot = 'C:\UECSS',
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$automationTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$projectFile = Join-Path $ProjectRoot 'FactoryGame.uproject'
$wwisePlugin = Join-Path $ProjectRoot 'Plugins\Wwise\Wwise.uplugin'
$modFile = Join-Path $ProjectRoot 'Mods\GameFeatures\NodeUpgrade\NodeUpgrade.uplugin'
foreach ($requiredFile in @($buildTool, $automationTool, $projectFile, $wwisePlugin, $modFile)) {
    if (-not (Test-Path -LiteralPath $requiredFile)) {
        throw "Build prerequisite missing: $requiredFile"
    }
}

# Build the editor first, then use the same packaging command as Alpakit's UI.
& $buildTool FactoryEditor Win64 Development "-Project=$projectFile" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $LASTEXITCODE" }

& $automationTool "-ScriptsForProject=$projectFile" PackagePlugin "-project=$projectFile" `
    -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=NodeUpgrade `
    -build -platform=Win64 -server -serverplatform=Win64 -nocompileeditor -installed -merge
if ($LASTEXITCODE -ne 0) { throw "Alpakit packaging failed: $LASTEXITCODE" }

$archivePath = Join-Path $ProjectRoot 'Saved\ArchivedPlugins\NodeUpgrade'
foreach ($archiveName in @('NodeUpgrade-Windows.zip', 'NodeUpgrade-WindowsServer.zip', 'NodeUpgrade.zip')) {
    $archiveFile = Join-Path $archivePath $archiveName
    if (-not (Test-Path -LiteralPath $archiveFile)) { throw "Expected archive missing: $archiveFile" }
    Get-Item -LiteralPath $archiveFile | Select-Object FullName, Length, LastWriteTime
}
