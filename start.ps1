$root = $PSScriptRoot

# Load backend/.env if it exists
$envFile = "$root\backend\.env"
if (Test-Path $envFile) {
    foreach ($line in Get-Content $envFile) {
        if ($line -match '^\s*([^#][^=]+)=(.*)$') {
            $key   = $Matches[1].Trim()
            $value = $Matches[2].Trim()
            [System.Environment]::SetEnvironmentVariable($key, $value, "Process")
            Write-Host "  Loaded $key" -ForegroundColor DarkGray
        }
    }
    Write-Host "Loaded backend/.env" -ForegroundColor Cyan
}

Write-Host "Starting TraceMind..." -ForegroundColor Cyan

$backend = Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "cd '$root\backend'; python -m uvicorn app.main:app --reload --port 8000" `
    -PassThru

$frontend = Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "cd '$root\frontend'; npm run dev" `
    -PassThru

Write-Host "Backend  -> http://localhost:8000" -ForegroundColor Green
Write-Host "Frontend -> http://localhost:5173" -ForegroundColor Green
Write-Host ""
Write-Host "Press Ctrl+C here to stop both servers." -ForegroundColor Yellow

try {
    Wait-Process -Id $backend.Id, $frontend.Id
} finally {
    Stop-Process -Id $backend.Id  -ErrorAction SilentlyContinue
    Stop-Process -Id $frontend.Id -ErrorAction SilentlyContinue
}