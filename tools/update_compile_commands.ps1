$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = (Resolve-Path -LiteralPath $repoRoot).Path
$toolsDir = Join-Path $repoRoot 'tools'
$srcDir = Join-Path $repoRoot 'src'
$vscodeDir = Join-Path $repoRoot '.vscode'
$dryRunLog = Join-Path $vscodeDir 'make-dryrun-full.log'
$outFile = Join-Path $vscodeDir 'compile_commands.json'
$tmpOutFile = Join-Path $vscodeDir 'compile_commands.json.tmp'
$mingwBin = Join-Path (Join-Path (Split-Path $repoRoot -Parent) 'MinGW') 'bin'
$gxx = Join-Path $mingwBin 'g++.exe'
$gcc = Join-Path $mingwBin 'gcc.exe'

$null = New-Item -ItemType Directory -Force -Path $vscodeDir

$cmd = 'cd /d "{0}" && call oosvars_mingw.bat && cd /d "{1}" && make -B -n all > "{2}"' -f $toolsDir, $srcDir, $dryRunLog
cmd.exe /c $cmd | Out-Null

$dirStack = New-Object System.Collections.Generic.Stack[string]
$entries = New-Object System.Collections.Generic.List[object]
$enteringPattern = 'Entering directory `(.+)'''
$leavingPattern = 'Leaving directory `(.+)'''
$sourcePattern = '(?<!\S)([^"\s]+\.(?:c|cpp))(?=\s|$)'
$includePattern = '-I(?:"([^"]+)"|([^\s]+))'

Get-Content -LiteralPath $dryRunLog -Encoding Default | ForEach-Object {
    $line = $_.TrimEnd()

    if ($line -match $enteringPattern) {
        $dirStack.Push($matches[1])
        return
    }

    if ($line -match $leavingPattern) {
        if ($dirStack.Count -gt 0) {
            $dirStack.Pop() | Out-Null
        }
        return
    }

    if ($dirStack.Count -eq 0) {
        return
    }

    if ($line -notmatch '^(g\+\+|gcc)\s+') {
        return
    }

    $sourceMatch = [regex]::Match($line, $sourcePattern)
    if (-not $sourceMatch.Success) {
        return
    }

    $sourceFile = $sourceMatch.Groups[1].Value.Replace('/', '\')
    $currentDir = (Resolve-Path -LiteralPath $dirStack.Peek()).Path
    $fullFile = (Resolve-Path -LiteralPath (Join-Path $currentDir $sourceFile)).Path

    $command = $line
    if ($command.StartsWith('g++ ')) {
        $command = '"' + $gxx + '" ' + $command.Substring(4)
    } elseif ($command.StartsWith('gcc ')) {
        $command = '"' + $gcc + '" ' + $command.Substring(4)
    }

    $command = [regex]::Replace(
        $command,
        $includePattern,
        {
            param($match)

            $includePath = $match.Groups[1].Value
            if (-not $includePath) {
                $includePath = $match.Groups[2].Value
            }

            if ([System.IO.Path]::IsPathRooted($includePath)) {
                $fullIncludePath = $includePath
            } else {
                $fullIncludePath = (Resolve-Path -LiteralPath (Join-Path $currentDir $includePath)).Path
            }

            '-I"{0}"' -f $fullIncludePath
        }
    )

    $sourceTokenPattern = '(?<!\S)' + [regex]::Escape($sourceMatch.Groups[1].Value) + '(?=\s|$)'
    $command = [regex]::Replace($command, $sourceTokenPattern, '"' + $fullFile + '"', 1)

    $entries.Add([pscustomobject]@{
        directory = $repoRoot
        command = $command
        file = $fullFile
    })
}

$entries | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $tmpOutFile -Encoding utf8
Move-Item -LiteralPath $tmpOutFile -Destination $outFile -Force

Write-Output ("Wrote {0} entries to {1}" -f $entries.Count, $outFile)
