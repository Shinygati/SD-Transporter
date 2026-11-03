$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore,WindowsBase
$iosRoot = Split-Path $PSScriptRoot -Parent
[xml]$svgDoc = Get-Content -LiteralPath (Join-Path $iosRoot 'Artwork/launcher.svg') -Raw
$converter = New-Object System.Windows.Media.BrushConverter
foreach ($size in @(40,60,58,87,80,120,180,1024)) {
    $visual = New-Object System.Windows.Media.DrawingVisual
    $drawing = $visual.RenderOpen()
    $drawing.PushTransform((New-Object System.Windows.Media.ScaleTransform(($size/108.0),($size/108.0))))
    $drawing.DrawRectangle($converter.ConvertFromString('#123C39'),$null,(New-Object System.Windows.Rect(0,0,108,108)))
    foreach ($node in $svgDoc.DocumentElement.ChildNodes) {
        if ($node.LocalName -eq 'path') {
            $drawing.DrawGeometry($converter.ConvertFromString($node.GetAttribute('fill')),$null,[System.Windows.Media.Geometry]::Parse($node.GetAttribute('d')))
        }
    }
    $drawing.Pop(); $drawing.Close()
    $bitmap = New-Object System.Windows.Media.Imaging.RenderTargetBitmap($size,$size,96,96,[System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $opaque = New-Object System.Windows.Media.Imaging.FormatConvertedBitmap($bitmap,[System.Windows.Media.PixelFormats]::Bgr32,$null,0)
    $encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($opaque))
    $stream = [System.IO.File]::Create((Join-Path $iosRoot "Walker/Assets.xcassets/AppIcon.appiconset/icon-$size.png"))
    try { $encoder.Save($stream) } finally { $stream.Dispose() }
}
Write-Output 'Rendered opaque iOS icons from original vector source.'
