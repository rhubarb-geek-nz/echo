// Copyright (c) 2025 Roger Brown.
// Licensed under the MIT License.

package nz.geek.rhubarb.socket;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;

public class Main {

  public static void main(String[] args) throws IOException, InterruptedException {
    try (Socket socket = new Socket()) {
      socket.connect(
          new InetSocketAddress(
              InetAddress.getByName(args.length > 0 ? args[0] : "localhost"),
              args.length > 1 ? Integer.valueOf(args[1]) : 7));

      Thread thread =
          new Thread(
              () -> {
                try (InputStream is = socket.getInputStream()) {
                  copy(is, System.out);
                } catch (IOException ex) {
                }
              });

      thread.start();

      try (OutputStream os = socket.getOutputStream()) {
        copy(System.in, os);
        socket.shutdownOutput();
        thread.join();
      }
    }
  }

  static void copy(InputStream is, OutputStream os) throws IOException {
    byte[] data = new byte[4096];
    while (true) {
      int i = is.read(data);
      if (i < 1) break;
      os.write(data, 0, i);
    }
  }
}
