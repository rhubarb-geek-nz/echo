#!/usr/bin/env pwsh
# Copyright (c) 2025 Roger Brown.
# Licensed under the MIT License.

function Invoke-Socket
{
	[CmdletBinding()]
	param(
		[string]$address = '127.0.0.1',
		[int]$port = 7,
		[Parameter(ValueFromPipeline,Mandatory)][byte[]]$InputObject
	)
	begin
	{
		$stream = [System.Net.Sockets.SocketType]::Stream
		$tcp = [System.Net.Sockets.ProtocolType]::Tcp
		$socket = New-Object -TypeName 'System.Net.Sockets.Socket' -ArgumentList $stream, $tcp
		$socket.Connect($address,$port)
		$job = Start-ThreadJob -ScriptBlock {
			Param($socket)
			$ba = New-Object -TypeName 'System.Byte[]' -ArgumentList (,4096)
			while ($true)
			{
				$len = $socket.Receive($ba)
				if ($len -lt 1)
				{
					break
				}
				if ($ba.length -eq $len)
				{
					Write-Output -InputObject $ba -NoEnumerate
					$ba = New-Object -TypeName 'System.Byte[]' -ArgumentList (,4096)
				}
				else
				{
					$seg = New-Object -TypeName 'System.Byte[]' -ArgumentList (,$len)
					[System.Array]::Copy($ba,0,$seg,0,$len)
					Write-Output -InputObject $seg -NoEnumerate
				}
			}
		} -ArgumentList $socket
	}
	process
	{
		$len = $socket.Send($InputObject)
		if ($len -ne $InputObject.length)
		{
			Write-Error "Send length wrong, $len -ne $($InputObject.length)"
		}	
	}
	end
	{
		try
		{
			$send = [System.Net.Sockets.SocketShutdown]::Send
			$socket.Shutdown($send)
			Receive-Job -Job $job -Wait
		}
		finally
		{
			$socket.Dispose()
		}
	}
}
