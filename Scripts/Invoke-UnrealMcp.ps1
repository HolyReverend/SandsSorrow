param([Parameter(Mandatory=$true)][string]$RequestPath)
$ErrorActionPreference = 'Stop'
$endpoint = 'http://127.0.0.1:8000/mcp'
$init = @{jsonrpc='2.0';id=1;method='initialize';params=@{protocolVersion='2024-11-05';capabilities=@{};clientInfo=@{name='SandsOfSorrowTools';version='1.0'}}} | ConvertTo-Json -Depth 10
$response = Invoke-WebRequest -Uri $endpoint -Method Post -ContentType 'application/json' -Headers @{Accept='application/json, text/event-stream'} -Body $init
$headers = @{Accept='application/json, text/event-stream'; 'Mcp-Session-Id'=([string]($response.Headers['Mcp-Session-Id'] | Select-Object -First 1))}
$result = Invoke-RestMethod -Uri $endpoint -Method Post -ContentType 'application/json' -Headers $headers -Body ([System.IO.File]::ReadAllText($RequestPath))
$result | ConvertTo-Json -Depth 100
