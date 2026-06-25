param(
  [string]$NamePattern = 'ST|STM|MBED|Serial|COM'
)

$ErrorActionPreference = 'Continue'

Write-Host '== STM32 Windows probe =='
Write-Host ''

Write-Host '== USB / PnP devices =='
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match $NamePattern -or $_.DeviceID -match 'VID_0483|PID_374B' } |
  Select-Object Name, Status, DeviceID |
  Format-Table -AutoSize

Write-Host ''
Write-Host '== Signed driver bindings =='
Get-CimInstance Win32_PnPSignedDriver |
  Where-Object { $_.DeviceName -match $NamePattern -or $_.DeviceID -match 'VID_0483|PID_374B' } |
  Select-Object DeviceName, DriverProviderName, DriverVersion, InfName, DeviceID |
  Format-Table -AutoSize

Write-Host ''
Write-Host '== Serial ports =='
try {
  [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
} catch {
  Write-Warning $_
}

Write-Host ''
Write-Host '== Tool commands =='
$cmds = @(
  'openocd',
  'arm-none-eabi-gcc',
  'arm-none-eabi-gdb',
  'make',
  'cmake',
  'ninja',
  'pio',
  'stm32pio',
  'pyocd',
  'zadig',
  'STM32_Programmer_CLI'
)

foreach ($cmd in $cmds) {
  $found = Get-Command $cmd -ErrorAction SilentlyContinue
  if ($found) {
    "{0}`t{1}" -f $cmd, $found.Source
  } else {
    "{0}`tMISSING" -f $cmd
  }
}

Write-Host ''
Write-Host '== Notes =='
Write-Host 'If OpenOCD reports libusb_open() failed, bind only the ST-Link Debug MI_00 interface to WinUSB using Zadig.'
Write-Host 'Do not replace the COM/VCP driver or mass-storage/MBED driver.'
