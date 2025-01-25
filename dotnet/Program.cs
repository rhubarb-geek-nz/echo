// Copyright (c) 2025 Roger Brown.
// Licensed under the MIT License.

using System;
using System.IO;
using System.Net.Sockets;
using System.Threading.Tasks;

using (Socket socket = new Socket(SocketType.Stream, ProtocolType.Tcp))
{
    await socket.ConnectAsync(
        args.Length >= 1 ? args[0] : "localhost",
        args.Length >= 2 ? Int32.Parse(args[1]) : 7);

    Task task = ReceiveTask.RunAsync(socket);

    try
    {
        using (Stream stdin = Console.OpenStandardInput())
        {
            byte[] buffer = new byte[1024];

            while (true)
            {
                int i = await stdin.ReadAsync(buffer, 0, buffer.Length);

                if (i == 0) break;

                int j = 0;

                while (j < i)
                {
                    int k = await socket.SendAsync(new ArraySegment<byte>(buffer, j, i - j));

                    if (k < 1) throw new IOException($"send returned {k}");

                    j += k;
                }
            }
        }
    }
    finally
    {
        socket.Shutdown(SocketShutdown.Send);
    }

    await task;
}

internal class ReceiveTask
{
    internal static async Task RunAsync(Socket socket)
    {
        using (Stream stdout = Console.OpenStandardOutput())
        {
            byte[] buffer = new byte[1024];

            while (true)
            {
                int i = await socket.ReceiveAsync(buffer);

                if (i < 1)
                {
                    break;
                }

                await stdout.WriteAsync(buffer, 0, i);
            }
        }
    }
}
