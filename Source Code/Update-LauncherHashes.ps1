$ErrorActionPreference='Stop'
$launcher=Join-Path $PSScriptRoot 'Launcher/ManualLauncher.cs'
$text=[IO.File]::ReadAllText($launcher)
foreach($name in @('JentaSpecialEdition.dll','DideMeleeNative.dll','DideMeleeDurability.dll')){
 $hash=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot ('Builds/'+$name))).Hash.ToLowerInvariant()
 $pattern='(FileHash\(Path.Combine\(game,@"Jenta_Special_Edition/Native/'+[regex]::Escape($name)+'"\)\)!=")[0-9a-f]{64}'
 if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Expected one binary hash'}
 $text=[regex]::Replace($text,$pattern,('$'+'{1}'+$hash))
}
[IO.File]::WriteAllText($launcher,$text)
