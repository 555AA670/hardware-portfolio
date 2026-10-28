param(
    [ValidateSet('CLEANBUCK36_BENCH')][string]$Profile='CLEANBUCK36_BENCH',
    [switch]$All,
    [string]$CompilerRoot='C:\Keil_v5\ARM\ARMCC\bin'
)
$ErrorActionPreference='Stop'
$compilerRoot=$CompilerRoot
$projectPath=Join-Path $PSScriptRoot 'MDK-ARM\CleanBuck36.uvprojx'
if (!(Test-Path -LiteralPath $projectPath)) { throw 'Missing checked-in project' }
[xml]$project=Get-Content -LiteralPath $projectPath -Raw
$profiles=@($Profile)
function Invoke-ArmTool([string]$Tool,[string[]]$Arguments) {
    $result=& (Join-Path $compilerRoot $Tool) @Arguments 2>&1
    $code=$LASTEXITCODE
    $result | Out-File -LiteralPath $script:buildLog -Append -Encoding utf8
    if ($code -ne 0 -or ($result -join "`n") -match '(?i)\bwarning\b|\berror\b') {
        throw "$Tool failed (exit $code): $($result -join "`n")"
    }
}
Push-Location (Join-Path $PSScriptRoot 'MDK-ARM')
try {
    New-Item -ItemType Directory -Force -Path Logs | Out-Null
    foreach ($name in $profiles) {
        $target=@($project.Project.Targets.Target | Where-Object TargetName -eq $name)[0]
        if (!$target) { throw "Missing target $name" }
        $started=Get-Date
        $script:buildLog=Join-Path $PWD ('Logs\'+$name+'-'+$started.ToString('yyyyMMdd-HHmmss-fff')+'.log')
        "ARMCC5 independent full rebuild: $name" | Set-Content -LiteralPath $script:buildLog
        $output='Build/'+$name
        New-Item -ItemType Directory -Force -Path $output | Out-Null
        $control=$target.TargetOption.TargetArmAds.Cads.VariousControls
        $argsC=@('--cpu','Cortex-M4.fp','--c99','-O2','--apcs=interwork','--split_sections','-g')
        foreach ($define in $control.Define.Split(',')) { $argsC+='-D'+$define }
        foreach ($include in $control.IncludePath.Split(';')) { $argsC+='-I'+$include }
        $objects=@(); $hashes=@()
        foreach ($group in $target.Groups.Group) {
            foreach ($file in $group.Files.File) {
                $source=[string]$file.FilePath
                $object=$output+'/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.o'
                if ($source -match '\.s$') {
                    Invoke-ArmTool 'armasm.exe' @('--cpu','Cortex-M4.fp','--apcs=interwork','-g','-o',$object,$source)
                } elseif ($source -match '\.c$') {
                    $unitArgs=$argsC
                    if($source -match 'cascade_control\.c$') {
                        # Same IEEE arithmetic and coefficients; optimize this
                        #50kHz module for execution time, not minimum code size.
                        $unitArgs=@($argsC|Where-Object{$_ -ne '-O2'})+@('-O3','-Otime')
                    }
                    Invoke-ArmTool 'armcc.exe' ($unitArgs+@('-c',$source,'-o',$object))
                } else { throw "Unexpected source: $source" }
                $objects+=$object
                $hashes+=@{File=$source;SHA256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash}
            }
        }
        $axf="$output/$name.axf"
        Invoke-ArmTool 'armlink.exe' (@('--cpu','Cortex-M4.fp','--strict','--scatter','stm32g474xx_flash.sct','--map','--callgraph','--symbols','--info','sizes,totals,unused,veneers','--list',"$output/$name.map",'-o',$axf)+$objects)
        Invoke-ArmTool 'fromelf.exe' @('--i32combined','--output',"$output/$name.hex",$axf)
        Invoke-ArmTool 'fromelf.exe' @('--text','-c','--output',"$output/$name.disasm",$axf)
        $hex=(Resolve-Path -LiteralPath "$output/$name.hex").Path
        if ((Get-Item -LiteralPath $hex).LastWriteTime -lt $started) { throw "Stale HEX $hex" }
        foreach ($header in Get-ChildItem -LiteralPath '../Core/Inc' -Filter '*.h') {
            $hashes+=@{File='../Core/Inc/'+$header.Name;SHA256=(Get-FileHash -LiteralPath $header.FullName -Algorithm SHA256).Hash}
        }
        foreach ($cascadeHeader in @('../../Simulation/CurrentCascade/cascade_control.h','../../Simulation/CurrentCascade/cascade_coefficients.h')) {
            $hashes+=@{File=$cascadeHeader;SHA256=(Get-FileHash -LiteralPath $cascadeHeader -Algorithm SHA256).Hash}
        }
        $hashes+=@{File='stm32g474xx_flash.sct';SHA256=(Get-FileHash -LiteralPath 'stm32g474xx_flash.sct' -Algorithm SHA256).Hash}
        $hash=(Get-FileHash -LiteralPath $hex -Algorithm SHA256).Hash
        [pscustomobject]@{Profile=$name;Defines=$control.Define;ControllerOptimization='-O3 -Otime (no fast math)';Hex=[IO.Path]::GetFileName($hex);SHA256=$hash;BuiltAt=(Get-Date).ToString('O');Log=('Logs/'+[IO.Path]::GetFileName($script:buildLog));Sources=$hashes;Flashed=$false} |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$output/manifest.json" -Encoding utf8
        '0 Error(s), 0 Warning(s)' | Add-Content -LiteralPath $script:buildLog
        "$name : 0 errors, 0 warnings; SHA256=$hash"
    }
} finally { Pop-Location }
