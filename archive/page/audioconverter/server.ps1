$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$port = 8787
$ip = '127.0.0.1'

$mime = @{
  '.html' = 'text/html; charset=utf-8'
  '.js'   = 'text/javascript; charset=utf-8'
  '.wasm' = 'application/wasm'
  '.json' = 'application/json; charset=utf-8'
  '.css'  = 'text/css; charset=utf-8'
  '.md'   = 'text/plain; charset=utf-8'
}

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://${ip}:${port}/")
try { $listener.Start() } catch {
  Write-Host "端口 $port 被占用，改用 8790"
  $port = 8790
  $listener.Prefixes.Clear()
  $listener.Prefixes.Add("http://${ip}:${port}/")
  $listener.Start()
}
Write-Host "音频转 MP3 已启动： http://${ip}:${port}/"
Start-Process "http://${ip}:${port}/"

while ($listener.IsListening) {
  $ctx = $listener.GetContext()
  $req = $ctx.Request
  $res = $ctx.Response
  try {
    $p = [Uri]::UnescapeDataString($req.Url.AbsolutePath)
    if ($p -eq '/') { $p = '/index.html' }
    $target = Join-Path $root ($p -replace '^[\\/]+', '')
    if (-not $target.StartsWith($root)) { throw 'forbidden' }
    if (-not (Test-Path $target -PathType Leaf)) { throw 'notfound' }
    $ext = [IO.Path]::GetExtension($target).ToLower()
    $res.ContentType = if ($mime.ContainsKey($ext)) { $mime[$ext] } else { 'application/octet-stream' }
    # 关键响应头：开启 cross-origin isolated
    $res.Headers.Add('Cross-Origin-Opener-Policy', 'same-origin')
    $res.Headers.Add('Cross-Origin-Embedder-Policy', 'require-corp')
    $res.Headers.Add('Cross-Origin-Resource-Policy', 'cross-origin')
    $res.Headers.Add('Cache-Control', 'no-cache')
    $bytes = [IO.File]::ReadAllBytes($target)
    $res.ContentLength64 = $bytes.Length
    $res.OutputStream.Write($bytes, 0, $bytes.Length)
  } catch {
    $res.StatusCode = 404
    $msg = [Text.Encoding]::UTF8.GetBytes('404 Not Found')
    $res.OutputStream.Write($msg, 0, $msg.Length)
  } finally {
    $res.OutputStream.Close()
  }
}
