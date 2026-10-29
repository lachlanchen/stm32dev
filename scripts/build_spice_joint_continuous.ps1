param([switch]$Flash)
$ErrorActionPreference='Stop'
$base=Join-Path $PSScriptRoot '..\firmware\stm32_sensor_head_lcd'
$base=[IO.Path]::GetFullPath($base)
$old=Join-Path $base 'build_spice_dual_lcd_capture'
$objects=@(Get-ChildItem -LiteralPath $old -Filter '*.o' -File -Recurse |
    Where-Object {$_.BaseName -notlike '*main'} |
    ForEach-Object {$_.FullName.Substring($base.Length+1).Replace('\','/')})
if($objects.Count -lt 5){throw 'Preserved DCP1 common objects are required; do not replace old build'}
& make -C $base -f Makefile.joint_continuous 'PROJECT=stm32_spice_joint_continuous' `
    'BUILD_DIR=build_spice_joint_continuous' ('JOINT_COMMON='+($objects -join ' ')) joint-continuous
if($LASTEXITCODE -ne 0){throw 'Joint-continuous build failed'}
if($Flash){
    $elf=Join-Path $base 'build_spice_joint_continuous\stm32_spice_joint_continuous.elf'
    & 'C:\ProgramData\chocolatey\lib\openocd\tools\install\bin\openocd.exe' `
        -f interface/stlink.cfg -c 'adapter serial 00710049572D430E4E4C3054' `
        -f target/stm32h7x.cfg -c 'adapter speed 1000' `
        -c ('program {'+$elf.Replace('\','/')+'} verify reset exit')
    if($LASTEXITCODE -ne 0){throw 'Flash/verification failed; original backup remains preserved'}
}
