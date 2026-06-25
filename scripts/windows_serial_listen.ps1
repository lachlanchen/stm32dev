param(
  [string]$Port = 'COM7',
  [int]$Baud = 115200,
  [int]$Seconds = 10
)

$ErrorActionPreference = 'Stop'

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$serial.ReadTimeout = 500
$deadline = (Get-Date).AddSeconds($Seconds)

try {
  $serial.Open()
  Write-Host "Listening on $Port at $Baud baud for $Seconds seconds..."
  while ((Get-Date) -lt $deadline) {
    try {
      $line = $serial.ReadLine()
      Write-Output $line
    } catch [System.TimeoutException] {
      Start-Sleep -Milliseconds 50
    }
  }
} finally {
  if ($serial.IsOpen) {
    $serial.Close()
  }
}
