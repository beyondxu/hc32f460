# Repairs only the exact autostash conflict reviewed for the cooldown update.
# Run from the HC32 Git checkout. No reset, checkout, stash pop/drop or commit.
$ErrorActionPreference = 'Stop'

$repoRoot = (& git rev-parse --show-toplevel)
if ($LASTEXITCODE -ne 0) { throw 'Run this script inside the HC32 Git checkout.' }
$repoRoot = $repoRoot.Trim()
$driverRel = 'projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/source/tph_m0333n.c'
$coreRel = 'projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/source/tph_continuous_ce.inc'
$unmerged = @(& git -C $repoRoot diff --name-only --diff-filter=U)
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the Git conflict list.' }
if ($unmerged.Count -ne 1 -or $unmerged[0] -cne $driverRel) {
    throw 'Conflict list differs from the reviewed case. No files changed; send git status.'
}

function Read-GitCore([string] $revision) {
    $coreLines = @(& git -C $repoRoot show "${revision}:$coreRel")
    if ($LASTEXITCODE -ne 0) { throw "Cannot read reference $revision. No files changed." }
    return ($coreLines -join "`n")
}
function Normalize-Core([string] $value) {
    return ([regex]::Replace($value, '\r\n?', "`n")).Trim()
}

$driverPath = Join-Path $repoRoot $driverRel
$driverBytes = [IO.File]::ReadAllBytes($driverPath)
# One-byte decoding preserves all original bytes, including GBK comments.
$byteEncoding = [Text.Encoding]::GetEncoding(28591)
$driverText = $byteEncoding.GetString($driverBytes)
$conflictPattern = '(?m)^<<<<<<< Updated upstream\r?\n(?<upstream>[\s\S]*?)^=======\r?\n(?<local>[\s\S]*?)^>>>>>>> Stashed changes(?:\r?\n|$)'
$conflictBlocks = [regex]::Matches($driverText, $conflictPattern)
if ($conflictBlocks.Count -ne 1) {
    throw 'Expected exactly one Updated upstream / Stashed changes block. No files changed.'
}
$conflictBlock = $conflictBlocks[0]
$oldReference = Read-GitCore 'fe9de20'
$newReference = Read-GitCore 'ccbad8c'
if ((Normalize-Core $conflictBlock.Groups['local'].Value) -cne (Normalize-Core $oldReference)) {
    throw 'Local conflict block contains additional edits. No files changed; send git diff --cc.'
}
if ((Normalize-Core $conflictBlock.Groups['upstream'].Value) -cne (Normalize-Core $newReference)) {
    throw 'Upstream conflict block differs from the reviewed cooldown version. No files changed.'
}

$resolvedText = $driverText.Substring(0, $conflictBlock.Index) +
    $conflictBlock.Groups['upstream'].Value +
    $driverText.Substring($conflictBlock.Index + $conflictBlock.Length)
if ([regex]::IsMatch($resolvedText, '(?m)^(<<<<<<<|=======|>>>>>>>)')) {
    throw 'Additional conflict markers found. No files changed.'
}
$samplePattern = '(?s)\binline\s+void\s+delay_ts_400ns\s*\(void\)\s*\{(?<body>.*?)\r?\n\}'
$sampleBefore = [regex]::Match($driverText, $samplePattern)
$sampleAfter = [regex]::Match($resolvedText, $samplePattern)
if (-not $sampleBefore.Success -or -not $sampleAfter.Success -or
    $sampleBefore.Value -cne $sampleAfter.Value) {
    throw 'Sampling helper preservation check failed. No files changed.'
}
$sampleNops = [regex]::Matches($sampleAfter.Groups['body'].Value, '\bnop\b').Count

$gitDir = (& git -C $repoRoot rev-parse --absolute-git-dir)
if ($LASTEXITCODE -ne 0) { throw 'Cannot locate Git backup directory. No files changed.' }
$backupDir = Join-Path $gitDir.Trim() ('tph-conflict-backups/' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($backupDir) | Out-Null
$backupPath = Join-Path $backupDir 'tph_m0333n.c.conflicted.bak'
[IO.File]::WriteAllBytes($backupPath, $driverBytes)
[IO.File]::WriteAllBytes($driverPath, $byteEncoding.GetBytes($resolvedText))
& git -C $repoRoot add -- $driverRel
if ($LASTEXITCODE -ne 0) {
    throw "Source repaired but git add failed. Original backup: $backupPath"
}
Write-Host 'Resolved the reviewed cooldown autostash conflict.'
Write-Host "Sampling helper preserved byte-for-byte; NOP count: $sampleNops"
Write-Host "Full original source backup: $backupPath"
Write-Host 'Only the driver conflict was marked resolved; other files and stashes were retained.'
Write-Host 'Next: git merge --autostash --no-edit origin/codex/printer-column-cooldown'
