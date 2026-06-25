param(
  [int]$AdapterSpeed = 1000,
  [string]$InterfaceConfig = 'interface/stlink.cfg',
  [string]$TargetConfig = 'target/stm32h7x.cfg'
)

$ErrorActionPreference = 'Stop'

$openocd = Get-Command openocd -ErrorAction Stop

& $openocd.Source `
  -f $InterfaceConfig `
  -f $TargetConfig `
  -c "adapter speed $AdapterSpeed" `
  -c 'init' `
  -c 'halt' `
  -c 'targets' `
  -c 'reg pc' `
  -c 'mdw 0x08000000 8' `
  -c 'resume' `
  -c 'shutdown'
