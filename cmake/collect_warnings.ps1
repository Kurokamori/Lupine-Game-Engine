<#
.SYNOPSIS
    Extracts compiler warnings and errors from a raw build log into a grouped,
    deduplicated report keyed by source file.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $InputLog,
    [Parameter(Mandatory = $true)] [string] $OutputLog
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputLog)) {
    Write-Error "Build log not found: $InputLog"
    exit 1
}

# MSVC:  C:\path\file.cpp(120,5): warning C4244: conversion ... [C:\path\proj.vcxproj]
# clang: C:\path\file.cpp:120:5: warning: unused variable 'x'
$msvcPattern  = '^\s*(?<file>[^()]+?)\((?<line>\d+)(?:,(?<col>\d+))?\)\s*:\s*(?<kind>warning|error|fatal error)\s+(?<code>[A-Za-z]+\d+)\s*:\s*(?<message>.*?)(?:\s*\[[^\[\]]*\])?\s*$'
$clangPattern = '^\s*(?<file>[A-Za-z]:[^:]+|[^:]+)\:(?<line>\d+)\:(?<col>\d+)\:\s*(?<kind>warning|error|fatal error)\s*:\s*(?<message>.*)$'

$records = New-Object System.Collections.Generic.List[object]
$seen    = New-Object System.Collections.Generic.HashSet[string]

foreach ($rawLine in [System.IO.File]::ReadLines($InputLog)) {
    $match = [regex]::Match($rawLine, $msvcPattern)
    $code  = ''
    if (-not $match.Success) {
        $match = [regex]::Match($rawLine, $clangPattern)
    } else {
        $code = $match.Groups['code'].Value
    }
    if (-not $match.Success) { continue }

    $file = $match.Groups['file'].Value.Trim()
    # Skip third-party noise from vcpkg / external dependencies.
    if ($file -match '\\vcpkg[\\_]|\\external\\|/vcpkg[/_]|/external/') { continue }

    $record = [pscustomobject]@{
        File    = $file
        Line    = [int] $match.Groups['line'].Value
        Column  = if ($match.Groups['col'].Success) { [int] $match.Groups['col'].Value } else { 0 }
        Kind    = $match.Groups['kind'].Value.ToLowerInvariant()
        Code    = $code
        Message = $match.Groups['message'].Value.Trim()
    }

    $key = '{0}|{1}|{2}|{3}|{4}' -f $record.File, $record.Line, $record.Column, $record.Code, $record.Message
    if ($seen.Add($key)) { $records.Add($record) }
}

$errors   = @($records | Where-Object { $_.Kind -ne 'warning' })
$warnings = @($records | Where-Object { $_.Kind -eq 'warning' })

$out = New-Object System.Collections.Generic.List[string]
$out.Add('Lupine Engine build diagnostics')
$out.Add(('Source log : {0}' -f $InputLog))
$out.Add(('Generated  : {0}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')))
$out.Add(('Errors     : {0}' -f $errors.Count))
$out.Add(('Warnings   : {0}  (unique, project sources only)' -f $warnings.Count))
$out.Add('')

if ($warnings.Count -gt 0) {
    $out.Add('--- Warning counts by code ---')
    foreach ($group in ($warnings | Group-Object Code | Sort-Object Count -Descending)) {
        $label = if ([string]::IsNullOrWhiteSpace($group.Name)) { '(uncoded)' } else { $group.Name }
        $out.Add(('  {0,-10} {1,5}' -f $label, $group.Count))
    }
    $out.Add('')
}

foreach ($section in @(
    @{ Title = '=== ERRORS ==='; Items = $errors },
    @{ Title = '=== WARNINGS ==='; Items = $warnings }
)) {
    if ($section.Items.Count -eq 0) { continue }
    $out.Add($section.Title)
    foreach ($fileGroup in ($section.Items | Group-Object File | Sort-Object Name)) {
        $out.Add('')
        $out.Add(('{0}  ({1})' -f $fileGroup.Name, $fileGroup.Count))
        foreach ($item in ($fileGroup.Group | Sort-Object Line, Column)) {
            $code = if ([string]::IsNullOrWhiteSpace($item.Code)) { '' } else { ' ' + $item.Code }
            $out.Add(('  {0}:{1}:{2}:{3} {4}' -f $fileGroup.Name, $item.Line, $item.Column, $code, $item.Message))
        }
    }
    $out.Add('')
}

if ($records.Count -eq 0) {
    $out.Add('No warnings or errors found in project sources.')
}

$outDir = Split-Path -Parent $OutputLog
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}
[System.IO.File]::WriteAllLines($OutputLog, $out, [System.Text.UTF8Encoding]::new($false))

Write-Host ("Diagnostics: {0} error(s), {1} warning(s) -> {2}" -f $errors.Count, $warnings.Count, $OutputLog)
