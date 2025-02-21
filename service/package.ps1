# Copyright (c) 2025 Roger Brown.
# Licensed under the MIT License.

param(
	$CertificateThumbprint = '601A8B683F791E51F647D34AD102C38DA4DDB65F'
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

trap
{
	throw $PSItem
}

foreach ($EDITION in 'Community', 'Professional')
{
	$VCVARSDIR = "${Env:ProgramFiles}\Microsoft Visual Studio\2022\$EDITION\VC\Auxiliary\Build"

	if ( Test-Path -LiteralPath $VCVARSDIR -PathType Container )
	{
		break
	}
}

$VCVARSARM = 'vcvarsarm.bat'
$VCVARSARM64 = 'vcvarsarm64.bat'
$VCVARSAMD64 = 'vcvars64.bat'
$VCVARSX86 = 'vcvars32.bat'
$VCVARSHOST = 'vcvars32.bat'

switch ($Env:PROCESSOR_ARCHITECTURE)
{
	'AMD64' {
		$VCVARSX86 = 'vcvarsamd64_x86.bat'
		$VCVARSARM = 'vcvarsamd64_arm.bat'
		$VCVARSARM64 = 'vcvarsamd64_arm64.bat'
		$VCVARSHOST = $VCVARSAMD64
	}
	'ARM64' {
		$VCVARSX86 = 'vcvarsarm64_x86.bat'
		$VCVARSARM = 'vcvarsarm64_arm.bat'
		$VCVARSAMD64 = 'vcvarsarm64_amd64.bat'
		$VCVARSHOST = $VCVARSARM64
	}
	'X86' {
		$VCVARSXARM64 = 'vcvarsx86_arm64.bat'
		$VCVARSARM = 'vcvarsx86_arm.bat'
		$VCVARSAMD64 = 'vcvarsx86_amd64.bat'
	}
	Default {
		throw "Unknown architecture $Env:PROCESSOR_ARCHITECTURE"
	}
}

$VCVARSARCH = @{'arm' = $VCVARSARM; 'arm64' = $VCVARSARM64; 'x86' = $VCVARSX86; 'x64' = $VCVARSAMD64}

$ARCHLIST = ( $VCVARSARCH.Keys | ForEach-Object {
	$VCVARS = $VCVARSARCH[$_];
	if ( Test-Path -LiteralPath "$VCVARSDIR/$VCVARS" -PathType Leaf )
	{
		$_
	}
} | Sort-Object )

foreach ($DIR in 'obj', 'bin')
{
	if (Test-Path -LiteralPath $DIR)
	{
		Remove-Item -LiteralPath $DIR -Force -Recurse
	}
}

$ARCHLIST | ForEach-Object {
	$ARCH = $_

	$VCVARS = ( '{0}\{1}' -f $VCVARSDIR, $VCVARSARCH[$ARCH] )

	@"
CALL "$VCVARS"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO clean
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO CertificateThumbprint="$CertificateThumbprint"
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC"

	if ($LastExitCode -ne 0)
	{
		exit $LastExitCode
	}
}

if ($CertificateThumbprint)
{
	$codeSignCertificate = Get-ChildItem -path Cert:\ -Recurse -CodeSigningCert | Where-Object { $_.Thumbprint -eq $CertificateThumbprint }
}

foreach ($ARCH in $ARCHLIST)
{
	$VERSION=(Get-Item "bin\$ARCH\echo.exe").VersionInfo.ProductVersion

	$env:PRODUCTVERSION = $VERSION
	$env:PRODUCTARCH = $ARCH
	$env:PRODUCTWIN64 = 'yes'
	$env:PRODUCTPROGFILES = 'ProgramFiles64Folder'
	$env:INSTALLERVERSION = '500'

	switch ($ARCH)
	{
		'arm64' {
			$env:UPGRADECODE = 'C7863A52-D1DD-4E55-B0B4-1A250EB9D2BE'
		}

		'arm' {
			$env:UPGRADECODE = '5FCA263C-D933-47F5-B324-A411A86A25D9'
			$env:PRODUCTWIN64 = 'no'
			$env:PRODUCTPROGFILES = 'ProgramFilesFolder'
		}

		'x86' {
			$env:UPGRADECODE = '5063AE87-E5DE-4CBE-BB27-2C3A07AFAFD8'
			$env:PRODUCTWIN64 = 'no'
			$env:PRODUCTPROGFILES = 'ProgramFilesFolder'
			$env:INSTALLERVERSION = '200'
		}

		'x64' {
			$env:UPGRADECODE = 'B5B09368-E1BB-43CB-B6BD-642AC315C0C6'
			$env:INSTALLERVERSION = '200'
		}
	}	

	& "${env:WIX}bin\candle.exe" -nologo "echo.wxs"

	if ($LastExitCode -ne 0)
	{
		exit $LastExitCode
	}

	$MsiFilename = "echo-$VERSION-$ARCH.msi"

	& "${env:WIX}bin\light.exe" -nologo -cultures:null -out $MsiFilename 'echo.wixobj'

	if ($LastExitCode -ne 0)
	{
		exit $LastExitCode
	}

	Remove-Item 'echo.wix*'
	Remove-Item 'echo-*.wixpdb'

	if ($codeSignCertificate -and ($codeSignCertificate.Count -eq 1))
	{
		$null = Set-AuthenticodeSignature -FilePath $MsiFilename -HashAlgorithm 'SHA256' -Certificate $codeSignCertificate -TimestampServer 'http://timestamp.digicert.com'
	}
}
