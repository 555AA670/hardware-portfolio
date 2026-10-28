param(
 [string]$OutputDirectory=(Join-Path $PSScriptRoot '..\Results\input-cascade\nonlinear'),
 [string]$LccRoot='C:\Program Files\MATLAB\R2024b\sys\lcc64\lcc64',
 [string]$ArmClang='C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe'
)
$ErrorActionPreference='Stop'
$taskLccRoot=$LccRoot
if(-not [System.IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory=Join-Path $PSScriptRoot $OutputDirectory}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
Push-Location $PSScriptRoot
try {
 $sources=@('cascade_control.c','cascade_control.h','cascade_coefficients.h','simulate_cascade.c','cascade_tests.c','../Candidate/simulate_candidate.c','../Candidate/candidate_control.c','../Candidate/candidate_control.h','../../Firmware/Core/Src/control.c','../../Firmware/Core/Inc/control.h','../../Firmware/Core/Inc/bench_config.h')
 $hashes=@($sources|ForEach-Object{[pscustomobject]@{File=$_;SHA256=(Get-FileHash -LiteralPath $_).Hash}})
 foreach($pair in @(@('..\..\Firmware\Core\Src\control.c','old.obj'),@('..\Candidate\candidate_control.c','v4.obj'),@('cascade_control.c','cascade.obj'),@('simulate_cascade.c','plant.obj'))){
  $result=& "$taskLccRoot\bin\lcc64.exe" "-I$taskLccRoot\include64" '-I..\..\Firmware\Core\Inc' '-I..\Candidate' '-I.' -o $pair[1] $pair[0] 2>&1
  $result|Write-Output
  if($LASTEXITCODE -ne 0 -or ($result -join "`n") -match '(?i)\bwarning\b|\berror\b'){throw 'Native C compilation failed'}
 }
 & "$taskLccRoot\bin\lcclnk64.exe" "-L$taskLccRoot\lib64" -o simulate_cascade.exe old.obj v4.obj cascade.obj plant.obj
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 $result=& "$taskLccRoot\bin\lcc64.exe" "-I$taskLccRoot\include64" '-I.' -o tests.obj cascade_tests.c 2>&1
 if($LASTEXITCODE -ne 0 -or ($result -join "`n") -match '(?i)\bwarning\b|\berror\b'){throw "Unit compilation failed: $result"}
 & "$taskLccRoot\bin\lcclnk64.exe" "-L$taskLccRoot\lib64" -o cascade_tests.exe tests.obj cascade.obj
 if($LASTEXITCODE -ne 0){throw 'Unit link failed'}
 New-Item -ItemType Directory -Force -Path Build | Out-Null
 $armResult=& $ArmClang --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -std=c99 -O2 -Wall -Wextra -Werror -c cascade_control.c -o Build/cascade_control.o 2>&1
 $armResult|Write-Output
 if($LASTEXITCODE -ne 0){throw 'Cortex-M4 controller module compile failed'}
 & '.\cascade_tests.exe' (Join-Path $OutputDirectory 'current-impulse.csv')|Tee-Object -FilePath (Join-Path $OutputDirectory 'unit-tests.log')
 if($LASTEXITCODE -ne 0){throw 'Unit test failed'}
 & '.\simulate_cascade.exe' $OutputDirectory | Tee-Object -FilePath (Join-Path $OutputDirectory 'run.log')
 if($LASTEXITCODE -ne 0){throw 'Simulation process failed'}
 foreach($source in $hashes){if((Get-FileHash -LiteralPath $source.File).Hash -ne $source.SHA256){throw 'Source changed during run'}}
 [pscustomobject]@{BuiltAt=(Get-Date).ToString('O');Sources=$hashes;ExecutableSHA256=(Get-FileHash './simulate_cascade.exe').Hash;UnitExecutableSHA256=(Get-FileHash './cascade_tests.exe').Hash;CortexM4ModuleCompiled=$true;HardwareValidated=$false;Flashed=$false}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $OutputDirectory 'source-manifest.json') -Encoding utf8
}finally{Pop-Location}
