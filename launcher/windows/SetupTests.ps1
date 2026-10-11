$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Setup.ps1')
$script:checks = 0
function Assert-Setup($Condition, [string]$Description) {
    if (-not $Condition) { throw $Description }
    $script:checks++
}
$probe = Join-Path ([IO.Path]::GetTempPath()) ('jfg-stderr-' + [Guid]::NewGuid().ToString('N') + '.ps1')
[IO.File]::WriteAllText($probe, "[Console]::Error.WriteLine('fixture native stderr'); exit 7")
try {
    $script:nativeProbe = @()
    $rejected = $false
    try { Invoke-Checked "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" @('-NoProfile', '-File', $probe) | ForEach-Object { $script:nativeProbe += $_ } }
    catch { $rejected = $_.Exception.Message -like '*failed (7)*' }
    Assert-Setup $rejected 'Native stderr must preserve the actual command failure.'
    Assert-Setup ($script:nativeProbe -contains 'JFG-SUPPORT setup_exit=0x00000007') 'Native exit code was not included in diagnostics.'
    $negativeProbe = Invoke-SetupProbe "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" @('-NoProfile', '-File', $probe)
    Assert-Setup ($negativeProbe.ExitCode -eq 7) 'Missing prerequisite stderr must return its failure status without aborting setup.'
    Assert-Setup (@($negativeProbe.Lines).Count -eq 0) 'Native stderr must not be mistaken for an installed prerequisite.'
    Assert-Setup ($ErrorActionPreference -eq 'Stop') 'A prerequisite probe must restore the installer error preference.'

} finally { Remove-Item -LiteralPath $probe }
function Find-Git { if ($script:hasGit) { 'synthetic-git' } }
function Find-Python { if ($script:hasPython) { 'synthetic-python' } }
function Find-VisualStudio { if ($script:hasVs) { 'synthetic-vs' } }
function Test-Ubuntu { return $true }
function Test-LinuxPackages { return $script:hasLinux }
function Install-Package([string]$Id, [string]$Override = '') {
    $script:installs += $Id
    switch ($Id) {
        'Git.Git' { $script:hasGit = $true }
        'Python.Python.3.12' { $script:hasPython = $true }
        default { throw 'Unexpected installer in fixture' }
    }
}
function Invoke-Checked([string]$File, [string[]]$Arguments) {
    $script:commands += ,$Arguments
    if ($Arguments -contains 'install') { $script:hasLinux = $true }
}
$script:hasGit = $true; $script:hasPython = $true; $script:hasVs = $true; $script:hasLinux = $true
$script:installs = @(); $script:commands = @()
Ensure-BuildTools
Assert-Setup ($script:installs.Count -eq 0 -and $script:commands.Count -eq 0) 'A ready machine must not install anything.'
$script:hasGit = $false; $script:hasPython = $false; $script:hasLinux = $false
Ensure-BuildTools
Assert-Setup (($script:installs -join ',') -eq 'Git.Git,Python.Python.3.12') 'Only missing Windows tools should be installed.'
Assert-Setup ($script:commands.Count -eq 2) 'Ubuntu update/install sequence missing.'
Assert-Setup ($script:commands[0] -contains 'update') 'Ubuntu indexes must be updated first.'
Assert-Setup ($script:commands[1] -contains 'DEBIAN_FRONTEND=noninteractive') 'Ubuntu setup must not await invisible input.'
Assert-Setup ($script:commands[1] -contains 'gcc-multilib') 'Missing matching-ELF build prerequisite.'
$before = $script:installs.Count + $script:commands.Count
Ensure-BuildTools
Assert-Setup (($script:installs.Count + $script:commands.Count) -eq $before) 'Retry must reuse installed components.'
$CheckOnly = $false; $SourceCommit = 'a' * 40
$RomPath = Join-Path ([IO.Path]::GetTempPath()) ('jfg-invalid-' + [Guid]::NewGuid().ToString('N') + '.z64')
[IO.File]::WriteAllBytes($RomPath, [byte[]]@(1,2,3,4))
try {
    $rejected = $false
    try { Invoke-Setup } catch { $rejected = $_.Exception.Message -like '*supported 32 MiB*' }
    Assert-Setup $rejected 'Setup must reject an unsupported ROM before installation.'
    Assert-Setup (($script:installs.Count + $script:commands.Count) -eq $before) 'Bad ROM triggered a download.'
    $rejected = $false
    try { Get-PinnedSource '../main' } catch { $rejected = $_.Exception.Message -like '*source revision*' }
    Assert-Setup $rejected 'Mutable or path-like source revisions must be rejected.'
} finally { Remove-Item -LiteralPath $RomPath }
Write-Output "Setup checks passed: $script:checks"
